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

#ifndef ECSERIALIZER_H
#define ECSERIALIZER_H

#include <kopano/zcdefs.h>
#include <kopano/kcodes.h>

class IStream;

namespace KC {

class ECFifoBuffer;

#ifdef DEBUG
#define STR_DEF_TIMEOUT 0
#else
#define STR_DEF_TIMEOUT 600000
#endif

class ECSerializer {
public:
	virtual ~ECSerializer(void) _kc_impdtor;
	virtual ECRESULT SetBuffer(void *lpBuffer) = 0;

	virtual ECRESULT Write(const void *ptr, size_t size, size_t nmemb) = 0;
	virtual ECRESULT Read(void *ptr, size_t size, size_t nmemb) = 0;

	virtual ECRESULT Skip(size_t size, size_t nmemb) = 0;
	virtual ECRESULT Flush() = 0;
	
	virtual ECRESULT Stat(ULONG *lpulRead, ULONG *lpulWritten) = 0;
};

class _kc_export ECStreamSerializer _kc_final : public ECSerializer {
public:
	ECStreamSerializer(IStream *lpBuffer);
	_kc_hidden ECRESULT SetBuffer(void *) _kc_override;
	_kc_hidden ECRESULT Write(const void *ptr, size_t size, size_t nmemb) _kc_override;
	_kc_hidden ECRESULT Read(void *ptr, size_t size, size_t nmemb) _kc_override;
	_kc_hidden ECRESULT Skip(size_t size, size_t nmemb) _kc_override;
	_kc_hidden ECRESULT Flush(void) _kc_override;
	_kc_hidden ECRESULT Stat(ULONG *have_read, ULONG *have_written) _kc_override;

private:
	IStream *m_lpBuffer;
	ULONG m_ulRead = 0, m_ulWritten = 0;
};

class _kc_export ECFifoSerializer _kc_final : public ECSerializer {
public:
	enum eMode { serialize, deserialize };

	ECFifoSerializer(ECFifoBuffer *lpBuffer, eMode mode);
	_kc_hidden virtual ~ECFifoSerializer(void);
	_kc_hidden ECRESULT SetBuffer(void *) _kc_override;
	_kc_hidden ECRESULT Write(const void *ptr, size_t size, size_t nmemb) _kc_override;
	_kc_hidden ECRESULT Read(void *ptr, size_t size, size_t nmemb) _kc_override;
	_kc_hidden ECRESULT Skip(size_t size, size_t nmemb) _kc_override;
	_kc_hidden ECRESULT Flush(void) _kc_override;
	_kc_hidden ECRESULT Stat(ULONG *have_read, ULONG *have_written) _kc_override;

private:
	ECFifoBuffer *m_lpBuffer;
	eMode m_mode;
	ULONG m_ulRead = 0, m_ulWritten = 0;
};

} /* namespace */

#endif /* ECSERIALIZER_H */
