/*
 *	Logon/logoff time manager and asynchronous updater
 */
#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstdio>
#include <kopano/kcodes.h>
#include "logontime.hpp"
#include "ECDatabaseMySQL.h"
#include "ECSecurity.h"
#include "ECSession.h"
#include "edkmdb.h"

namespace KC {

static std::unordered_map<unsigned int, time_t> ltm_ontime_cache, ltm_offtime_cache;
static std::mutex ltm_ontime_mutex, ltm_offtime_mutex;

static ECRESULT ltm_sync_time(ECDatabase *db,
    const std::pair<unsigned int, time_t> &e, bool dir)
{
	FILETIME ft;
	UnixTimeToFileTime(e.second, &ft);
	std::string query = "SELECT hierarchy_id FROM stores WHERE stores.user_id=" + stringify(e.first);
	DB_RESULT result = NULL;
	ECRESULT ret = db->DoSelect(query, &result);
	if (ret != erSuccess)
		return ret;
	if (db->GetNumRows(result) == 0) {
		db->FreeResult(result);
		return erSuccess;
	}
	DB_ROW row = db->FetchRow(result);
	if (row == NULL) {
		db->FreeResult(result);
		return erSuccess;
	}
	unsigned int store_id = strtoul(row[0], NULL, 0);
	unsigned int prop = dir ? PR_LAST_LOGON_TIME : PR_LAST_LOGOFF_TIME;
	db->FreeResult(result);
	query = "REPLACE INTO properties (tag, type, hierarchyid, val_hi, val_lo) VALUES(" +
                stringify(PROP_ID(prop)) + "," + stringify(PROP_TYPE(prop)) + "," +
                stringify(store_id) + "," + stringify(ft.dwHighDateTime) + "," +
                stringify(ft.dwLowDateTime) + ")";
	return db->DoInsert(query);
}

void sync_logon_times(ECDatabase *db)
{
	/*
	 * Switchgrab the global map, so that we can run it to the database
	 * without holdings locks.
	 */
	bool failed = false;
	ltm_ontime_mutex.lock();
	decltype(ltm_ontime_cache) logon_time = std::move(ltm_ontime_cache);
	ltm_ontime_mutex.unlock();
	ltm_offtime_mutex.lock();
	decltype(ltm_offtime_cache) logoff_time = std::move(ltm_offtime_cache);
	ltm_offtime_mutex.unlock();
	if (logon_time.size() > 0 || logoff_time.size() > 0)
		ec_log_debug("Writing out logon/logoff time cache (%zu/%zu entries) to DB",
			logon_time.size(), logoff_time.size());
	for (const auto &i : logon_time)
		failed |= ltm_sync_time(db, i, 0) != erSuccess;
	for (const auto &i : logoff_time)
		failed |= ltm_sync_time(db, i, 1) != erSuccess;
	if (failed)
		ec_log_warn("Writeout of logon/off time cache unsuccessful");
}

/*
 * Save the current time as the last logon time for the logged-on user of
 * @ses.
 */
void record_logon_time(ECSession *ses, bool logon)
{
	unsigned int uid = ses->GetSecurity()->GetUserId();
	time_t now = time(NULL);
	if (logon) {
		ltm_ontime_mutex.lock();
		ltm_ontime_cache[uid] = now;
		ltm_ontime_mutex.unlock();
	} else {
		ltm_offtime_mutex.lock();
		ltm_offtime_cache[uid] = now;
		ltm_offtime_mutex.unlock();
	}
}

} /* namespace */
