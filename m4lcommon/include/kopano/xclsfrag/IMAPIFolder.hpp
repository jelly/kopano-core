virtual HRESULT __stdcall CreateMessage(LPCIID lpInterface, ULONG flags, LPMESSAGE *lppMessage) _kc_override;
virtual HRESULT __stdcall CopyMessages(LPENTRYLIST lpMsgList, LPCIID lpInterface, LPVOID lpDestFolder, ULONG ui_param, LPMAPIPROGRESS lpProgress, ULONG flags) _kc_override;
virtual HRESULT __stdcall DeleteMessages(LPENTRYLIST lpMsgList, ULONG ui_param, LPMAPIPROGRESS lpProgress, ULONG flags) _kc_override;
virtual HRESULT __stdcall CreateFolder(ULONG ulFolderType, LPTSTR lpszFolderName, LPTSTR lpszFolderComment, LPCIID lpInterface, ULONG flags, LPMAPIFOLDER *lppFolder) _kc_override;
virtual HRESULT __stdcall CopyFolder(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, LPVOID lpDestFolder, LPTSTR lpszNewFolderName, ULONG ui_param, LPMAPIPROGRESS lpProgress, ULONG flags) _kc_override;
virtual HRESULT __stdcall DeleteFolder(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ui_param, LPMAPIPROGRESS lpProgress, ULONG flags) _kc_override;
virtual HRESULT __stdcall SetReadFlags(LPENTRYLIST lpMsgList, ULONG ui_param, LPMAPIPROGRESS lpProgress, ULONG flags) _kc_override;
virtual HRESULT __stdcall GetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG flags, ULONG *lpulMessageStatus) _kc_override;
virtual HRESULT __stdcall SetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulNewStatus, ULONG ulNewStatusMask, ULONG *lpulOldStatus) _kc_override;
virtual HRESULT __stdcall SaveContentsSort(const SSortOrderSet *lpSortCriteria, ULONG flags) _kc_override;
virtual HRESULT __stdcall EmptyFolder(ULONG ui_param, LPMAPIPROGRESS lpProgress, ULONG flags) _kc_override;
