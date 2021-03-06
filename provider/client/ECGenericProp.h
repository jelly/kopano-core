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
 *
 */

#ifndef ECGENERICPROP_H
#define ECGENERICPROP_H

#include <kopano/zcdefs.h>
#include <mutex>
#include <kopano/ECUnknown.h>
#include "IECPropStorage.h"
#include "ECPropertyEntry.h"
#include <kopano/IECSingleInstance.h>

#include <list>
#include <map>
#include <set>

// These are the callback functions called when a software-handled property is requested
typedef HRESULT (*SetPropCallBack)(ULONG ulPropTag, void *lpProvider, const SPropValue *lpsPropValue, void *lpParam);
typedef HRESULT (* GetPropCallBack)(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);

struct PROPCALLBACK {
	ULONG ulPropTag;
	SetPropCallBack lpfnSetProp;
	GetPropCallBack lpfnGetProp;
	void *			lpParam;
	BOOL			fRemovable;
	BOOL			fHidden; // hidden from GetPropList

	bool operator==(const PROPCALLBACK &callback) const
	{
		return callback.ulPropTag == this->ulPropTag;
	}
};

typedef std::map<short, PROPCALLBACK>			ECPropCallBackMap;
typedef ECPropCallBackMap::iterator				ECPropCallBackIterator;
typedef std::map<short, ECPropertyEntry>		ECPropertyEntryMap;
typedef ECPropertyEntryMap::iterator			ECPropertyEntryIterator;

class ECGenericProp : public ECUnknown {
protected:
	ECGenericProp(void *lpProvider, ULONG ulObjType, BOOL fModify, const char *szClassName = NULL);
	virtual ~ECGenericProp();

public:
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface) _kc_override;

	HRESULT SetProvider(void* lpProvider);
	HRESULT SetEntryId(ULONG cbEntryId, LPENTRYID lpEntryId);

	static HRESULT		DefaultGetPropGetReal(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);	
	static HRESULT		DefaultGetPropNotFound(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);
	static HRESULT DefaultSetPropComputed(ULONG ulPropTag, void *lpProvider, const SPropValue *lpsPropValue, void *lpParam);
	static HRESULT DefaultSetPropIgnore(ULONG ulPropTag, void *lpProvider, const SPropValue *lpsPropValue, void *lpParam);
	static HRESULT DefaultSetPropSetReal(ULONG ulPropTag, void *lpProvider, const SPropValue *lpsPropValue, void *lpParam);
	static HRESULT		DefaultGetProp(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);

	// Our table-row getprop handler (handles client-side generation of table columns)
	static HRESULT		TableRowGetProp(void* lpProvider, struct propVal *lpsPropValSrc, LPSPropValue lpsPropValDst, void **lpBase, ULONG ulType);

	virtual HRESULT HrSetPropStorage(IECPropStorage *lpStorage, BOOL fLoadProps);
	virtual HRESULT HrSetRealProp(const SPropValue *lpsPropValue);
	virtual HRESULT		HrGetRealProp(ULONG ulPropTag, ULONG ulFlags, void *lpBase, LPSPropValue lpsPropValue, ULONG ulMaxSize = 0);
	virtual HRESULT		HrAddPropHandlers(ULONG ulPropTag, GetPropCallBack lpfnGetProp, SetPropCallBack lpfnSetProp, void *lpParam, BOOL fRemovable = FALSE, BOOL fHidden = FALSE);
	virtual HRESULT 	HrLoadEmptyProps();

	bool IsReadOnly() const;

protected: ///?
	virtual HRESULT		HrLoadProps();
	HRESULT				HrLoadProp(ULONG ulPropTag);
	virtual HRESULT		HrDeleteRealProp(ULONG ulPropTag, BOOL fOverwriteRO);
	HRESULT				HrGetHandler(ULONG ulPropTag, SetPropCallBack *lpfnSetProp, GetPropCallBack *lpfnGetProp, void **lpParam);
	HRESULT				IsPropDirty(ULONG ulPropTag, BOOL *lpbDirty);
	HRESULT				HrSetClean();
	HRESULT				HrSetCleanProperty(ULONG ulPropTag);

	/* used by child to save here in it's parent */
	friend class ECParentStorage;
	virtual HRESULT HrSaveChild(ULONG ulFlags, MAPIOBJECT *lpsMapiObject);
	virtual HRESULT HrRemoveModifications(MAPIOBJECT *lpsMapiObject, ULONG ulPropTag);

	// For IECSingleInstance
	virtual HRESULT GetSingleInstanceId(ULONG *lpcbInstanceID, LPSIEID *lppInstanceID);
	virtual HRESULT SetSingleInstanceId(ULONG cbInstanceID, LPSIEID lpInstanceID);

public:
	// From IMAPIProp
	virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
	virtual HRESULT SaveChanges(ULONG ulFlags);
	virtual HRESULT GetProps(const SPropTagArray *lpPropTagArray, ULONG ulFlags, ULONG *lpcValues, LPSPropValue *lppPropArray);
	virtual HRESULT GetPropList(ULONG ulFlags, LPSPropTagArray *lppPropTagArray);
	
	/**
	 * \brief Opens a property.
	 *
	 * \param ulPropTag				The proptag of the property to open.
	 * \param lpiid					Pointer to the requested interface for the object to be opened.
	 * \param ulInterfaceOptions	Interface options.
	 * \param ulFlags				Flags.
	 * \param lppUnk				Pointer to an IUnknown pointer where the opened object will be stored.
	 *
	 * \return hrSuccess on success.
	 */
	virtual HRESULT OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN *lppUnk);
	virtual HRESULT SetProps(ULONG cValues, const SPropValue *lpPropArray, LPSPropProblemArray *lppProblems);
	virtual HRESULT DeleteProps(const SPropTagArray *lpPropTagArray, LPSPropProblemArray *lppProblems);
	virtual HRESULT CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, const SPropTagArray *lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray *lppProblems);
	virtual HRESULT CopyProps(const SPropTagArray *lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray *lppProblems);
	virtual HRESULT GetNamesFromIDs(LPSPropTagArray *lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG *lpcPropNames, LPMAPINAMEID **lpppPropNames);
	virtual HRESULT GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID *lppPropNames, ULONG ulFlags, LPSPropTagArray *lppPropTags);

	class xMAPIProp _kc_final : public IMAPIProp {
		#include <kopano/xclsfrag/IUnknown.hpp>
		#include <kopano/xclsfrag/IMAPIProp.hpp>
	} m_xMAPIProp;

	class xECSingleInstance _kc_final : public IECSingleInstance {
		#include <kopano/xclsfrag/IUnknown.hpp>
		// <kopano/xclsfrag/IECSingleInstance.hpp>
		virtual HRESULT __stdcall GetSingleInstanceId(ULONG *lpcbInstanceID, LPENTRYID *lppInstanceID) _kc_override;
		virtual HRESULT __stdcall SetSingleInstanceId(ULONG cbInstanceID, LPENTRYID lpInstanceID) _kc_override;
	} m_xECSingleInstance;

protected:
	ECPropertyEntryMap *lstProps = nullptr;
	std::set<ULONG>			m_setDeletedProps;
	ECPropCallBackMap		lstCallBack;
	DWORD dwLastError = hrSuccess;
	BOOL fSaved = false; // only 0 if just created, // not saved until we either read or write from/to disk
	ULONG					ulObjType;
	ULONG ulObjFlags = 0; // message: MAPI_ASSOCIATED, folder: FOLDER_SEARCH (last?)

	BOOL					fModify;
	void*					lpProvider;
	BOOL isTransactedObject = true; // only ECMsgStore and ECMAPIFolder are not transacted
	ULONG m_ulMaxPropSize = 8192;

public:
	// Current entryid of object
	ULONG m_cbEntryId = 0;
	ENTRYID *m_lpEntryId = nullptr;
	MAPIOBJECT *m_sMapiObject = nullptr;
	std::recursive_mutex m_hMutexMAPIObject; /* Mutex for locking the MAPIObject */
	BOOL m_bReload = false, m_bLoading = false;
	IECPropStorage *lpStorage = nullptr;
};


// Inlines
inline bool ECGenericProp::IsReadOnly() const {
	return !fModify;
}

#endif
