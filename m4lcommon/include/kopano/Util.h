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

#ifndef UTIL_H
#define UTIL_H

#include <kopano/zcdefs.h>
#include <mapix.h>
#include <edkmdb.h>
#include <string>

#include <kopano/ECDefs.h>

#include <kopano/ustringutil.h>

namespace KC {

/*
 * This class only exists to satisfy the %extend directive in swig/libcommon.i.
 * It screams for replacement with a namespace Util {}
 */
class Util _kc_final {
	public:
	_kc_export static HRESULT HrAddToPropertyArray(const SPropValue *src, ULONG srcvals, const SPropValue *add, SPropValue **dest, ULONG *ndestvals);
	_kc_export static HRESULT HrMergePropertyArrays(const SPropValue *src, ULONG srcvals, const SPropValue *adds, ULONG naddvals, SPropValue **dest, ULONG *destvals);
	_kc_export static HRESULT HrCopyPropertyArray(const SPropValue *src, ULONG srcvals, LPSPropValue *dest, ULONG *destvals, bool excl_errors = false);
	static HRESULT	HrCopyPropertyArrayByRef(const SPropValue *lpSrc, ULONG cValues, LPSPropValue *lppDest, ULONG *cDestValues, bool bExcludeErrors = false);
	static HRESULT	HrCopyPropertyArray(const SPropValue *lpSrc, ULONG cValues, LPSPropValue lpDest, void *lpBase);
	static HRESULT	HrCopyPropertyArrayByRef(const SPropValue *lpSrc, ULONG cValues, LPSPropValue lpDest);
	_kc_export static HRESULT HrCopyProperty(LPSPropValue dest, const SPropValue *src, void *base, ALLOCATEMORE * = nullptr);
	static HRESULT	HrCopyPropertyByRef(LPSPropValue lpDest, const SPropValue *lpSrc);
	_kc_export static HRESULT HrCopySRestriction(LPSRestriction dst, const SRestriction *src, void *base);
	_kc_export static HRESULT  HrCopySRestriction(LPSRestriction *dst, const SRestriction *src);
	static HRESULT	HrCopyActions(ACTIONS *lpDest, const ACTIONS *lpSrc, void *lpBase);
	static HRESULT	HrCopyAction(ACTION *lpDest, const ACTION *lpSrc, void *lpBase);
	static HRESULT	HrCopySRowSet(LPSRowSet lpDest, const SRowSet *lpSrc, void *lpBase);
	_kc_export static HRESULT HrCopySRow(LPSRow dest, const SRow *src, void *base);
	_kc_export static HRESULT HrCopyPropTagArray(const SPropTagArray *src, LPSPropTagArray *dst);
	_kc_export static HRESULT HrCopyUnicodePropTagArray(ULONG flags, const SPropTagArray *src, LPSPropTagArray *dst);
	_kc_export static HRESULT HrCopyBinary(ULONG size, const BYTE *src, ULONG *destsize, LPBYTE *dest, LPVOID lpBase = nullptr);
	_kc_export static HRESULT HrCopyEntryId(ULONG size, const ENTRYID *src, ULONG *destsize, LPENTRYID *dest, LPVOID base = nullptr);
	_kc_export static int CompareSBinary(const SBinary &, const SBinary &);
	_kc_export static HRESULT CompareProp(const SPropValue *, const SPropValue *, const ECLocale &, int *res);
	static unsigned int PropSize(const SPropValue *lpProp);
	_kc_export static LONG FindPropInArray(const SPropTagArray *proptags, ULONG tag);
	_kc_export static HRESULT HrStreamToString(IStream *in, std::string &out);
	_kc_export static HRESULT HrStreamToString(IStream *in, std::wstring &out);
	static HRESULT HrConvertStreamToWString(IStream *sInput, ULONG ulCodepage, std::wstring *wstrOutput);
	_kc_export static HRESULT HrTextToRtf(IStream *text, IStream *rtf);
	_kc_export static HRESULT HrTextToHtml(IStream *text, IStream *html, ULONG codepage);
	_kc_export static HRESULT HrTextToHtml(const wchar_t *text, std::string &html, ULONG codepage);
	_kc_export static HRESULT HrHtmlToText(IStream *html, IStream *text, ULONG codepage);
	_kc_export static HRESULT HrHtmlToRtf(IStream *html, IStream *rtf, unsigned int codepage);
	static HRESULT	HrHtmlToRtf(const WCHAR *lpwHTML, std::string& strRTF);

	static ULONG GetBestBody(const SPropValue *lpBody, const SPropValue *lpHtml, const SPropValue *lpRtfCompressed, const SPropValue *lpRtfInSync, ULONG ulFlags);
	_kc_export static ULONG GetBestBody(IMAPIProp *obj, ULONG flags);
	_kc_export static ULONG GetBestBody(LPSPropValue props, ULONG nvals, ULONG flags);
	_kc_export static bool IsBodyProp(ULONG tag);
	_kc_export static HRESULT HrMAPIErrorToText(HRESULT, LPTSTR *err, void *base = nullptr);
	_kc_export static bool ValidatePropTagArray(const SPropTagArray *);
	static HRESULT bin2hex(ULONG inLength, const BYTE *input, char **output, void *parent = nullptr);
	_kc_export static HRESULT hex2bin(const char *input, size_t len, ULONG *outlen, LPBYTE *output, void *parent = nullptr);
	static HRESULT hex2bin(const char *input, size_t len, LPBYTE output);

	template <size_t N>
	static bool StrCaseCompare(const WCHAR *lpString, const WCHAR (&lpFind)[N], size_t pos = 0) {
		return wcsncasecmp(lpString + pos, lpFind, N-1) == 0;
	}

	/* DoCopyTo/DoCopyProps functions & their helpers */
	static HRESULT FindInterface(LPCIID lpIID, ULONG ulIIDs, LPCIID lpIIDs);
	static HRESULT CopyStream(LPSTREAM lpSrc, LPSTREAM lpDest);
	static HRESULT CopyRecipients(LPMESSAGE lpSrc, LPMESSAGE lpDest);
	static HRESULT CopyInstanceIds(LPMAPIPROP lpSrc, LPMAPIPROP lpDst);
	static HRESULT CopyAttachmentProps(LPATTACH lpSrcAttach, LPATTACH lpDestAttach, LPSPropTagArray lpExcludeProps = NULL);
	_kc_export static HRESULT CopyAttachments(LPMESSAGE src, LPMESSAGE dst, LPSRestriction r);
	static HRESULT CopyHierarchy(LPMAPIFOLDER lpSrc, LPMAPIFOLDER lpDest, ULONG ulFlags, ULONG ulUIParam, LPMAPIPROGRESS lpProgress);
	static HRESULT CopyContents(ULONG ulWhat, LPMAPIFOLDER lpSrc, LPMAPIFOLDER lpDest, ULONG ulFlags, ULONG ulUIParam, LPMAPIPROGRESS lpProgress);
	static HRESULT TryOpenProperty(ULONG ulPropType, ULONG ulSrcPropTag, LPMAPIPROP lpPropSrc, ULONG ulDestPropTag, LPMAPIPROP lpPropDest,
								   LPSTREAM *lppSrcStream, LPSTREAM *lppDestStream);
	static HRESULT AddProblemToArray(LPSPropProblem lpProblem, LPSPropProblemArray *lppProblems);
	_kc_export static HRESULT DoCopyTo(LPCIID src_intf, LPVOID src_obj, ULONG ciidExclude, LPCIID rgiidExclude, const SPropTagArray *exclprop, ULONG ui_param, LPMAPIPROGRESS, LPCIID dst_intf, LPVOID dst_obj, ULONG flags, LPSPropProblemArray *);
	_kc_export static HRESULT DoCopyProps(LPCIID src_intf, LPVOID src_obj, const SPropTagArray *inclprop, ULONG ui_param, LPMAPIPROGRESS, LPCIID dst_intf, LPVOID dst_obj, ULONG flags, LPSPropProblemArray *);
	_kc_export static HRESULT HrCopyIMAPData(LPMESSAGE src, LPMESSAGE dst);
	_kc_export static HRESULT HrDeleteIMAPData(LPMESSAGE);
	_kc_export static HRESULT HrGetQuotaStatus(IMsgStore *, ECQUOTA *, ECQUOTASTATUS **ret);
	_kc_export static HRESULT HrDeleteResidualProps(LPMESSAGE dstmsg, LPMESSAGE srcmsg, LPSPropTagArray valid_props);
	static HRESULT ValidMapiPropInterface(LPCIID lpInterface);
	static HRESULT QueryInterfaceMapiPropOrValidFallback(LPUNKNOWN lpInObj, LPCIID lpInterface, LPUNKNOWN *lppOutObj);

	static HRESULT HrFindEntryIDs(ULONG cbEID, LPENTRYID lpEID, ULONG cbEntryIDs, LPSPropValue lpEntryIDs, BOOL *lpbFound, ULONG* lpPos);
	_kc_export static HRESULT HrDeleteAttachments(LPMESSAGE);
	_kc_export static HRESULT HrDeleteRecipients(LPMESSAGE);
	_kc_export static HRESULT HrDeleteMessage(IMAPISession *, IMessage *);
	static bool FHasHTML(IMAPIProp *lpProp);

	struct SBinaryLess {
		bool operator()(const SBinary &left, const SBinary &right) const {
			return CompareSBinary(left, right) < 0;
		}
	};
	
	_kc_export static HRESULT ReadProperty(IMAPIProp *, ULONG tag, std::string &data);
	_kc_export static HRESULT WriteProperty(IMAPIProp *, ULONG tag, const std::string &data);
	static HRESULT ExtractRSSEntryID(LPSPropValue blob, ULONG *eid_size, LPENTRYID *eid);
	_kc_export static HRESULT ExtractSuggestedContactsEntryID(LPSPropValue prop_blob, ULONG *eid_size, LPENTRYID *eid);
	static HRESULT ExtractAdditionalRenEntryID(LPSPropValue lpPropBlob, unsigned short usBlockType, ULONG *lpcbEntryID, LPENTRYID *lppEntryID);
};

#define RTF_TAG_TYPE_TEXT	0x0000
#define RTF_TAG_TYPE_HTML	0x0010
#define RTF_TAG_TYPE_HEAD	0x0020
#define RTF_TAG_TYPE_BODY	0x0030
#define RTF_TAG_TYPE_P		0x0040
#define RTF_TAG_TYPE_STARTP	0x0050
#define RTF_TAG_TYPE_ENDP	0x0060
#define RTF_TAG_TYPE_BR		0x0070
#define RTF_TAG_TYPE_PRE	0x0080
#define RTF_TAG_TYPE_FONT	0x0090
#define RTF_TAG_TYPE_HEADER	0x00A0
#define RTF_TAG_TYPE_TITLE	0x00B0
#define RTF_TAG_TYPE_PLAIN	0x00C0
#define RTF_TAG_TYPE_UNK	0x00F0

#define RTF_INBODY			0x0000
#define RTF_INHEAD			0x0001
#define RTF_INHTML			0x0002
#define RTF_OUTHTML			0x0003

#define RTF_FLAG_INPAR		0x0004
#define RTF_FLAG_CLOSE		0x0008
#define RTF_FLAG_MHTML		0x0100

} /* namespace */

#endif // UTIL_H
