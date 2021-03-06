/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <kopano/platform.h>
#include <stdexcept>
#include <utility>
#include <kopano/lockhelper.hpp>
#include <kopano/memory.hpp>
#include <mapispi.h>
#include <mapix.h>
#include <kopano/ECDebug.h>
#include "ECMsgStore.h"
#include "ECNotifyClient.h"
#include "ECSessionGroupManager.h"
#include <kopano/ECGuid.h>
#include "SOAPUtils.h"
#include "WSUtil.h"
#include <kopano/Util.h>
#include <kopano/stringutil.h>
#include <kopano/mapiext.h>

#define MAX_NOTIFS_PER_CALL 64

struct ECADVISE {
	ULONG cbKey;
	BYTE *lpKey;
	ULONG ulEventMask;
	IMAPIAdviseSink *lpAdviseSink;
	ULONG ulConnection;
	GUID guid;
	ULONG ulSupportConnection;
};

struct ECCHANGEADVISE {
	ULONG ulSyncId;
	ULONG ulChangeId;
	ULONG ulEventMask;
	IECChangeAdviseSink *lpAdviseSink;
	ULONG ulConnection;
	GUID guid;
};

using namespace KCHL;

static inline std::pair<ULONG,ULONG> SyncAdviseToConnection(const SSyncAdvise &sSyncAdvise) {
	return std::make_pair(sSyncAdvise.sSyncState.ulSyncId,sSyncAdvise.ulConnection);
}

ECNotifyClient::ECNotifyClient(ULONG ulProviderType, void *lpProvider,
    ULONG ulFlags, LPMAPISUP lpSupport) :
	ECUnknown("ECNotifyClient"), m_lpSupport(lpSupport),
	m_lpProvider(lpProvider), m_ulProviderType(ulProviderType)
{
	TRACE_MAPI(TRACE_ENTRY, "ECNotifyClient::ECNotifyClient","");

	ECSESSIONID ecSessionId;

	if(m_ulProviderType == MAPI_STORE)
		m_lpTransport = ((ECMsgStore*)m_lpProvider)->lpTransport;
	else if(m_ulProviderType == MAPI_ADDRBOOK)
		m_lpTransport = ((ECABLogon*)m_lpProvider)->m_lpTransport;
	else
		throw std::runtime_error("Unknown m_ulProviderType");

    /* Get the sessiongroup ID of the provider that we will be handling notifications for */
	if (m_lpTransport->HrGetSessionId(&ecSessionId, &m_ecSessionGroupId) != hrSuccess)
		throw std::runtime_error("ECNotifyClient/HrGetSessionId failed");

    /* Get the session group that this session belongs to */
	if (g_ecSessionManager.GetSessionGroupData(m_ecSessionGroupId, m_lpTransport->GetProfileProps(), &m_lpSessionGroup) != hrSuccess)
		throw std::runtime_error("ECNotifyClient/GetSessionGroupData failed");

	if (m_lpSessionGroup->GetOrCreateNotifyMaster(&m_lpNotifyMaster) != hrSuccess)
		throw std::runtime_error("ECNotifyClient/GetOrCreateNotifyMaster failed");

	m_lpNotifyMaster->AddSession(this);
}

ECNotifyClient::~ECNotifyClient()
{
	TRACE_MAPI(TRACE_ENTRY, "ECNotifyClient::~ECNotifyClient","");

	if (m_lpNotifyMaster)
		m_lpNotifyMaster->ReleaseSession(this);

	if (m_lpSessionGroup)
		m_lpSessionGroup->Release();

    /*
     * We MAY have been the last person using the session group. Tell the session group manager
     * to look at the session group and delete it if necessary
     */
	g_ecSessionManager.DeleteSessionGroupDataIfOrphan(m_ecSessionGroupId);
	/*
	 * Clean up, this map should actually be empty if all advised were correctly unadvised.
	 * This is however not always the case, but ECNotifyMaster and Server will remove all
	 * advises when the session is removed.
	 */
	ulock_rec biglock(m_hMutex);
	for (const auto &i : m_mapAdvise) {
		if (i.second->lpAdviseSink != NULL)
			i.second->lpAdviseSink->Release();
		MAPIFreeBuffer(i.second);
	}
	m_mapAdvise.clear();

	for (const auto &i : m_mapChangeAdvise) {
		if (i.second->lpAdviseSink != NULL)
			i.second->lpAdviseSink->Release();
		MAPIFreeBuffer(i.second);
	}

	m_mapChangeAdvise.clear();
	biglock.unlock();
	TRACE_MAPI(TRACE_RETURN, "ECNotifyClient::~ECNotifyClient","");
}

HRESULT ECNotifyClient::Create(ULONG ulProviderType, void *lpProvider, ULONG ulFlags, LPMAPISUP lpSupport, ECNotifyClient**lppNotifyClient)
{
	ECNotifyClient *lpNotifyClient = new ECNotifyClient(ulProviderType, lpProvider, ulFlags, lpSupport);

	HRESULT hr = lpNotifyClient->QueryInterface(IID_ECNotifyClient, (void **)lppNotifyClient);
	if (hr != hrSuccess)
		delete lpNotifyClient;

	return hr;
}

HRESULT ECNotifyClient::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE2(ECNotifyClient, this);
	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

/**
 * Register an advise connection
 *
 * In windows, registers using the IMAPISupport subscription model, so that threading is handled correctly
 * concerning the multithreading model selected by the client when doing MAPIInitialize(). However, when
 * bSynchronous is TRUE, that notification is always handled internal synchronously while the notifications
 * are being received.
 *
 * @param[in] cbKey Bytes in lpKey
 * @param[in] lpKey Key to subscribe for
 * @param[in] ulEventMask Mask for events to receive
 * @param[in] TRUE if the notification should be handled synchronously, FALSE otherwise. In linux, handled as if
 *                 it were always TRUE
 * @param[in] lpAdviseSink Sink to send notifications to
 * @param[out] lpulConnection Connection ID for the subscription
 * @return result
 */
HRESULT ECNotifyClient::RegisterAdvise(ULONG cbKey, LPBYTE lpKey, ULONG ulEventMask, bool bSynchronous, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection)
{
	HRESULT		hr = MAPI_E_NO_SUPPORT;
	memory_ptr<ECADVISE> pEcAdvise;
	ULONG		ulConnection = 0;

	if (lpKey == nullptr)
		return MAPI_E_INVALID_PARAMETER;
	hr = MAPIAllocateBuffer(sizeof(ECADVISE), &~pEcAdvise);
	if (hr != hrSuccess)
		return hr;
	*lpulConnection = 0;

	memset(pEcAdvise, 0, sizeof(ECADVISE));
	
	pEcAdvise->lpKey = NULL;
	pEcAdvise->cbKey = cbKey;

	hr = MAPIAllocateMore(cbKey, pEcAdvise, (LPVOID*)&pEcAdvise->lpKey);
	if (hr != hrSuccess)
		return hr;
	memcpy(pEcAdvise->lpKey, lpKey, cbKey);
	
	pEcAdvise->lpAdviseSink	= lpAdviseSink;
	pEcAdvise->ulEventMask	= ulEventMask;
	pEcAdvise->ulSupportConnection = 0;

	/*
	 * Request unique connection id from Master.
	 */
	hr = m_lpNotifyMaster->ReserveConnection(&ulConnection);
	if(hr != hrSuccess)
		return hr;

	// Add reference on the notify sink
	lpAdviseSink->AddRef();

#ifdef NOTIFY_THROUGH_SUPPORT_OBJECT
	memory_ptr<NOTIFKEY> lpKeySupport;

	if(!bSynchronous) {
		hr = MAPIAllocateBuffer(CbNewNOTIFKEY(sizeof(GUID)), &~lpKeySupport);
		if(hr != hrSuccess)
			return hr;
		lpKeySupport->cb = sizeof(GUID);
		hr = CoCreateGuid((GUID *)lpKeySupport->ab);
		if(hr != hrSuccess)
			return hr;

		// Get support object connection id
		hr = m_lpSupport->Subscribe(lpKeySupport, (ulEventMask&~fnevLongTermEntryIDs), 0, lpAdviseSink, &pEcAdvise->ulSupportConnection);
		if(hr != hrSuccess)
			return hr;
		memcpy(&pEcAdvise->guid, lpKeySupport->ab, sizeof(GUID));
	}
#endif

	{
		scoped_rlock biglock(m_hMutex);
		m_mapAdvise.insert(ECMAPADVISE::value_type(ulConnection, pEcAdvise.release()));
	}

	// Since we're ready to receive notifications now, register ourselves with the master
	hr = m_lpNotifyMaster->ClaimConnection(this, &ECNotifyClient::Notify, ulConnection);
	if(hr != hrSuccess)
		return hr;
	// Set out value
	*lpulConnection = ulConnection;
	return hrSuccess;
}

HRESULT ECNotifyClient::RegisterChangeAdvise(ULONG ulSyncId, ULONG ulChangeId,
    IECChangeAdviseSink *lpChangeAdviseSink, ULONG *lpulConnection)
{
	HRESULT			hr = MAPI_E_NO_SUPPORT;
	memory_ptr<ECCHANGEADVISE> pEcAdvise;
	ULONG			ulConnection = 0;

	hr = MAPIAllocateBuffer(sizeof(ECCHANGEADVISE), &~pEcAdvise);
	if (hr != hrSuccess)
		return hr;
	*lpulConnection = 0;

	memset(pEcAdvise, 0, sizeof(ECCHANGEADVISE));
	
	pEcAdvise->ulSyncId = ulSyncId;
	pEcAdvise->ulChangeId = ulChangeId;
	pEcAdvise->lpAdviseSink = lpChangeAdviseSink;
	pEcAdvise->ulEventMask = fnevKopanoIcsChange;

	/*
	 * Request unique connection id from Master.
	 */
	hr = m_lpNotifyMaster->ReserveConnection(&ulConnection);
	if(hr != hrSuccess)
		return hr;
	/*
	 * Setup our maps to receive the notifications
	 */
	{
		scoped_rlock biglock(m_hMutex);
		lpChangeAdviseSink->AddRef();
		m_mapChangeAdvise.insert(ECMAPCHANGEADVISE::value_type(ulConnection, pEcAdvise.release()));
	}

	// Since we're ready to receive notifications now, register ourselves with the master
	hr = m_lpNotifyMaster->ClaimConnection(this, &ECNotifyClient::NotifyChange, ulConnection);
	if(hr != hrSuccess)
		return hr;
	// Set out value
	*lpulConnection = ulConnection;
	return hrSuccess;
}

HRESULT ECNotifyClient::UnRegisterAdvise(ULONG ulConnection)
{
	/*
	 * Release connection from Master
	 */
	HRESULT hr = m_lpNotifyMaster->DropConnection(ulConnection);
	if (hr != hrSuccess)
		return hr;

	// Remove notify from list
	scoped_rlock lock(m_hMutex);
	auto iIterAdvise = m_mapAdvise.find(ulConnection);
	if (iIterAdvise != m_mapAdvise.cend()) {
		if(iIterAdvise->second->ulSupportConnection)
			m_lpSupport->Unsubscribe(iIterAdvise->second->ulSupportConnection);

		if (iIterAdvise->second->lpAdviseSink != NULL)
			iIterAdvise->second->lpAdviseSink->Release();

		MAPIFreeBuffer(iIterAdvise->second);
		m_mapAdvise.erase(iIterAdvise);	
		return hr;
	}
	auto iIterChangeAdvise = m_mapChangeAdvise.find(ulConnection);
	if (iIterChangeAdvise == m_mapChangeAdvise.cend())
		return hr;
	if (iIterChangeAdvise->second->lpAdviseSink != NULL)
		iIterChangeAdvise->second->lpAdviseSink->Release();
	MAPIFreeBuffer(iIterChangeAdvise->second);
	m_mapChangeAdvise.erase(iIterChangeAdvise);
	return hr;
}

HRESULT ECNotifyClient::Advise(ULONG cbKey, LPBYTE lpKey, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection){

	TRACE_NOTIFY(TRACE_ENTRY, "ECNotifyClient::Advise", "");

	HRESULT		hr = MAPI_E_NO_SUPPORT;
	ULONG		ulConnection = 0;

	
	hr = RegisterAdvise(cbKey, lpKey, ulEventMask, false, lpAdviseSink, &ulConnection);
	if (hr != hrSuccess)
		goto exit;

	//Request the advice
	hr = m_lpTransport->HrSubscribe(cbKey, lpKey, ulConnection, ulEventMask);
	if(hr != hrSuccess) {
		UnRegisterAdvise(ulConnection);
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	} 
	
	// Set out value
	*lpulConnection = ulConnection;
	hr = hrSuccess;

exit:
	TRACE_NOTIFY(TRACE_RETURN, "ECNotifyClient::Advise", "hr=0x%08X connection=%d", hr, *lpulConnection);

	return hr;
}

HRESULT ECNotifyClient::Advise(const ECLISTSYNCSTATE &lstSyncStates,
    IECChangeAdviseSink *lpChangeAdviseSink, ECLISTCONNECTION *lplstConnections)
{
	TRACE_NOTIFY(TRACE_ENTRY, "ECNotifyClient::AdviseICS", "");

	HRESULT				hr = MAPI_E_NO_SUPPORT;
	ECLISTSYNCADVISE	lstAdvises;

	for (const auto &state : lstSyncStates) {
		SSyncAdvise sSyncAdvise = {{0}};

		hr = RegisterChangeAdvise(state.ulSyncId, state.ulChangeId, lpChangeAdviseSink, &sSyncAdvise.ulConnection);
		if (hr != hrSuccess)
			goto exit;
		sSyncAdvise.sSyncState = state;
		lstAdvises.push_back(std::move(sSyncAdvise));
	}

	hr = m_lpTransport->HrSubscribeMulti(lstAdvises, fnevKopanoIcsChange);
	if (hr != hrSuccess) {
		// On failure we'll try the one-at-a-time approach.
		for (auto iSyncAdvise = lstAdvises.cbegin();
		     iSyncAdvise != lstAdvises.cend(); ++iSyncAdvise) {
			hr = m_lpTransport->HrSubscribe(iSyncAdvise->sSyncState.ulSyncId, iSyncAdvise->sSyncState.ulChangeId, iSyncAdvise->ulConnection, fnevKopanoIcsChange);
			if (hr != hrSuccess) {
				// Unadvise all advised connections
				// No point in attempting the multi version as SubscribeMulti also didn't work
				for (auto iSyncUnadvise = lstAdvises.cbegin();
				     iSyncUnadvise != iSyncAdvise; ++iSyncUnadvise)
					m_lpTransport->HrUnSubscribe(iSyncUnadvise->ulConnection);
				
				hr = MAPI_E_NO_SUPPORT;
				goto exit;
			} 
		}
	}

	std::transform(lstAdvises.begin(), lstAdvises.end(), std::back_inserter(*lplstConnections), &SyncAdviseToConnection);

exit:
	if (hr != hrSuccess) {
		// Unregister all advises.
		for (auto iSyncAdvise = lstAdvises.cbegin();
		     iSyncAdvise != lstAdvises.cend(); ++iSyncAdvise)
			UnRegisterAdvise(iSyncAdvise->ulConnection);
	}

	TRACE_NOTIFY(TRACE_RETURN, "ECNotifyClient::AdviseICS", "hr=0x%08X", hr);
	return hr;
}

HRESULT ECNotifyClient::Unadvise(ULONG ulConnection)
{
	TRACE_NOTIFY(TRACE_ENTRY, "ECNotifyClient::Unadvise", "%d", ulConnection);

	HRESULT hr	= MAPI_E_NO_SUPPORT;

	// Logoff the advisor
	hr = m_lpTransport->HrUnSubscribe(ulConnection);
	if (hr != hrSuccess)
		goto exit;

	hr = UnRegisterAdvise(ulConnection);
	if (hr != hrSuccess)
		goto exit;

exit:
	TRACE_NOTIFY(TRACE_RETURN, "ECNotifyClient::Unadvise", "hr=0x%08X", hr);

	return hr;
}

HRESULT ECNotifyClient::Unadvise(const ECLISTCONNECTION &lstConnections)
{
	TRACE_NOTIFY(TRACE_ENTRY, "ECNotifyClient::Unadvise", "");

	HRESULT hr	= MAPI_E_NO_SUPPORT;
	HRESULT hrTmp;
	bool bWithErrors = false;

	// Logoff the advisors
	hr = m_lpTransport->HrUnSubscribeMulti(lstConnections);
	if (hr != hrSuccess) {
		hr = hrSuccess;

		for (const auto &p : lstConnections) {
			hrTmp = m_lpTransport->HrUnSubscribe(p.second);
			if (FAILED(hrTmp))
				bWithErrors = true;
		}
	}
	for (const auto &p : lstConnections) {
		hrTmp = UnRegisterAdvise(p.second);
		if (FAILED(hrTmp))
			bWithErrors = true;
	}

	if (SUCCEEDED(hr) && bWithErrors)
		hr = MAPI_W_ERRORS_RETURNED;

	TRACE_NOTIFY(TRACE_RETURN, "ECNotifyClient::Unadvise", "hr=0x%08X", hr);

	return hr;
}

// Re-registers a notification on the server. Normally only called if the server
// session has been reset.
HRESULT ECNotifyClient::Reregister(ULONG ulConnection, ULONG cbKey, LPBYTE lpKey)
{
	scoped_rlock biglock(m_hMutex);

	ECMAPADVISE::const_iterator iter = m_mapAdvise.find(ulConnection);
	if (iter == m_mapAdvise.cend())
		return MAPI_E_NOT_FOUND;

	if(cbKey) {
		// Update key if required, when the new key is equal or smaller
		// then the previous key we don't need to allocate anything.
		// Note that we cannot do MAPIFreeBuffer() since iter->second->lpKey
		// was allocated with MAPIAllocateMore().
		if (cbKey > iter->second->cbKey) {
			HRESULT hr = MAPIAllocateMore(cbKey, iter->second,
				reinterpret_cast<void **>(&iter->second->lpKey));
			if (hr != hrSuccess)
				return hr;
		}

		memcpy(iter->second->lpKey, lpKey, cbKey);
		iter->second->cbKey = cbKey;
	}
	return m_lpTransport->HrSubscribe(iter->second->cbKey,
	       iter->second->lpKey, ulConnection, iter->second->ulEventMask);
}

HRESULT ECNotifyClient::ReleaseAll()
{
	scoped_rlock biglock(m_hMutex);

	for (auto &p : m_mapAdvise) {
		p.second->lpAdviseSink->Release();
		p.second->lpAdviseSink = NULL;
	}
	return hrSuccess;
}

typedef std::list<NOTIFICATION *> NOTIFICATIONLIST;
typedef std::list<SBinary *> BINARYLIST;

HRESULT ECNotifyClient::NotifyReload()
{
	HRESULT hr = hrSuccess;
	struct notification notif;
	struct notificationTable table;
	NOTIFYLIST notifications;

	memset(&notif, 0, sizeof(notif));
	memset(&table, 0, sizeof(table));

	notif.ulEventType = fnevTableModified;
	notif.tab = &table;
	notif.tab->ulTableEvent = TABLE_RELOAD;
	
	notifications.push_back(&notif);

	// The transport used for this notifyclient *may* have a broken session. Inform the
	// transport that the session may be broken and it should verify that all is well.

	// Disabled because deadlock, research needed
	//m_lpTransport->HrEnsureSession();

	// Don't send the notification while we are locked
	scoped_rlock biglock(m_hMutex);
	for (const auto &p : m_mapAdvise)
		if (p.second->cbKey == 4)
			Notify(p.first, notifications);
	return hr;
}

HRESULT ECNotifyClient::Notify(ULONG ulConnection, const NOTIFYLIST &lNotifications)
{
	HRESULT						hr = hrSuccess;
	ECMAPADVISE::const_iterator iterAdvise;
	NOTIFICATIONLIST			notifications;

	for (auto notp : lNotifications) {
		LPNOTIFICATION tmp = NULL;

		hr = CopySOAPNotificationToMAPINotification(m_lpProvider, notp, &tmp);
		if (hr != hrSuccess)
			continue;

		TRACE_NOTIFY(TRACE_ENTRY, "ECNotifyClient::Notify", "id=%d\n%s", notp->ulConnection, NotificationToString(1, tmp).c_str());
		notifications.push_back(tmp);
	}

	ulock_rec biglock(m_hMutex);

	/* Search for the right connection */
	iterAdvise = m_mapAdvise.find(ulConnection);
	if (iterAdvise == m_mapAdvise.cend() ||
	    iterAdvise->second->lpAdviseSink == NULL) {
		TRACE_NOTIFY(TRACE_WARNING, "ECNotifyClient::Notify", "Unknown Notification id %d", ulConnection);
		goto exit;
	}

	if (!notifications.empty()) {
		/* Send notifications in batches of MAX_NOTIFS_PER_CALL notifications */
		auto iterNotification = notifications.cbegin();
		while (iterNotification != notifications.cend()) {
			memory_ptr<NOTIFICATION> lpNotifs;
			/* Create a straight array of all the notifications */
			hr = MAPIAllocateBuffer(sizeof(NOTIFICATION) * MAX_NOTIFS_PER_CALL, &~lpNotifs);
			if (hr != hrSuccess)
				continue;

			ULONG i = 0;
			while (iterNotification != notifications.cend() && i < MAX_NOTIFS_PER_CALL) {
				/* We can do a straight memcpy here because pointers are still intact */
				memcpy(&lpNotifs[i++], *iterNotification, sizeof(NOTIFICATION));
				++iterNotification;
			}

			/* Send notification to the listener */
			if (!iterAdvise->second->ulSupportConnection) {
				if (iterAdvise->second->lpAdviseSink->OnNotify(i, lpNotifs) != 0)
					TRACE_NOTIFY(TRACE_WARNING, "ECNotifyClient::Notify", "Error by notify a client");
			} else {
				memory_ptr<NOTIFKEY> lpKey;
				ULONG		ulResult = 0;

				hr = MAPIAllocateBuffer(CbNewNOTIFKEY(sizeof(GUID)), &~lpKey);
				if (hr != hrSuccess)
					goto exit;

				lpKey->cb = sizeof(GUID);
				memcpy(lpKey->ab, &iterAdvise->second->guid, sizeof(GUID));

				// FIXME log errors
				m_lpSupport->Notify(lpKey, i, lpNotifs, &ulResult);
			}
		}
	}
exit:
	biglock.unlock();
	/* Release all notifications */
	for (auto notp : notifications)
		MAPIFreeBuffer(notp);
	return hr;
}

HRESULT ECNotifyClient::NotifyChange(ULONG ulConnection, const NOTIFYLIST &lNotifications)
{
	HRESULT						hr = hrSuccess;
	memory_ptr<ENTRYLIST> lpSyncStates;
	ECMAPCHANGEADVISE::const_iterator iterAdvise;
	BINARYLIST					syncStates;
	ulock_rec biglock(m_hMutex, std::defer_lock_t());

	/* Create a straight array of MAX_NOTIFS_PER_CALL sync states */
	hr = MAPIAllocateBuffer(sizeof *lpSyncStates, &~lpSyncStates);
	if (hr != hrSuccess)
		return hr;
	memset(lpSyncStates, 0, sizeof *lpSyncStates);

	hr = MAPIAllocateMore(sizeof *lpSyncStates->lpbin * MAX_NOTIFS_PER_CALL, lpSyncStates, (void**)&lpSyncStates->lpbin);
	if (hr != hrSuccess)
		return hr;
	memset(lpSyncStates->lpbin, 0, sizeof *lpSyncStates->lpbin * MAX_NOTIFS_PER_CALL);

	for (auto notp : lNotifications) {
		LPSBinary	tmp = NULL;

		hr = CopySOAPChangeNotificationToSyncState(notp, &tmp, lpSyncStates);
		if (hr != hrSuccess)
			continue;

		TRACE_NOTIFY(TRACE_ENTRY, "ECNotifyClient::NotifyChange", "id=%d\n%s", notp->ulConnection, bin2hex(tmp->cb, tmp->lpb).c_str());
		syncStates.push_back(tmp);
	}

	/* Search for the right connection */
	biglock.lock();
	iterAdvise = m_mapChangeAdvise.find(ulConnection);
	if (iterAdvise == m_mapChangeAdvise.cend() ||
	    iterAdvise->second->lpAdviseSink == NULL) {
		TRACE_NOTIFY(TRACE_WARNING, "ECNotifyClient::NotifyChange", "Unknown Notification id %d", ulConnection);
		return hr;
	}

	if (!syncStates.empty()) {
		/* Send notifications in batches of MAX_NOTIFS_PER_CALL notifications */
		auto iterSyncStates = syncStates.cbegin();
		while (iterSyncStates != syncStates.cend()) {

			lpSyncStates->cValues = 0;
			while (iterSyncStates != syncStates.cend() &&
			       lpSyncStates->cValues < MAX_NOTIFS_PER_CALL) {
				/* We can do a straight memcpy here because pointers are still intact */
				memcpy(&lpSyncStates->lpbin[lpSyncStates->cValues++], *iterSyncStates, sizeof *lpSyncStates->lpbin);
				++iterSyncStates;
			}

			/* Send notification to the listener */
			if (iterAdvise->second->lpAdviseSink->OnNotify(0, lpSyncStates) != 0)
				TRACE_NOTIFY(TRACE_WARNING, "ECNotifyClient::NotifyChange", "Error by notify a client");

		}
	}
	return hrSuccess;
}

HRESULT ECNotifyClient::UpdateSyncStates(const ECLISTSYNCID &lstSyncId, ECLISTSYNCSTATE *lplstSyncState)
{
	return m_lpTransport->HrGetSyncStates(lstSyncId, lplstSyncState);
}
