virtual HRESULT __stdcall Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection) _kc_override;
virtual HRESULT __stdcall Unadvise(ULONG ulConnection) _kc_override;
virtual HRESULT __stdcall CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG flags, ULONG *lpulResult) _kc_override;
virtual HRESULT __stdcall OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG flags, ULONG *lpulObjType, LPUNKNOWN *lppUnk) _kc_override;
virtual HRESULT __stdcall SetReceiveFolder(LPTSTR lpszMessageClass, ULONG flags, ULONG cbEntryID, LPENTRYID lpEntryID) _kc_override;
virtual HRESULT __stdcall GetReceiveFolder(LPTSTR lpszMessageClass, ULONG flags, ULONG *lpcbEntryID, LPENTRYID *lppEntryID, LPTSTR *lppszExplicitClass) _kc_override;
virtual HRESULT __stdcall GetReceiveFolderTable(ULONG flags, LPMAPITABLE *lppTable) _kc_override;
virtual HRESULT __stdcall StoreLogoff(ULONG *lpulFlags) _kc_override;
virtual HRESULT __stdcall AbortSubmit(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG flags) _kc_override;
virtual HRESULT __stdcall GetOutgoingQueue(ULONG flags, LPMAPITABLE *lppTable) _kc_override;
virtual HRESULT __stdcall SetLockState(LPMESSAGE lpMessage,ULONG ulLockState) _kc_override;
virtual HRESULT __stdcall FinishedMsg(ULONG flags, ULONG cbEntryID, LPENTRYID lpEntryID) _kc_override;
virtual HRESULT __stdcall NotifyNewMail(LPNOTIFICATION lpNotification) _kc_override;
