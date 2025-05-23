# Generated by devtools/yamaker.

LIBRARY()

LICENSE(
    Apache-2.0 AND
    BSL-1.0 AND
    CC0-1.0 AND
    MIT
)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

VERSION(24.8.14.39)

PEERDIR(
    contrib/clickhouse/base/poco/Crypto
    contrib/clickhouse/base/poco/Net
    contrib/clickhouse/base/poco/Util
)

ADDINCL(
    GLOBAL contrib/clickhouse/base/poco/NetSSL_OpenSSL/include
    contrib/clickhouse/base/poco/Crypto/include
    contrib/clickhouse/base/poco/Foundation/include
    contrib/clickhouse/base/poco/Net/include
    contrib/clickhouse/base/poco/Util/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

IF (OS_DARWIN)
    CFLAGS(
        GLOBAL -DOS_DARWIN
    )
ELSEIF (OS_LINUX)
    CFLAGS(
        GLOBAL -DOS_LINUX
    )
ENDIF()

IF (OS_LINUX)
    CFLAGS(
        -DPOCO_HAVE_FD_EPOLL
    )
ENDIF()

CFLAGS(
    -DPOCO_ENABLE_CPP11
    -DPOCO_OS_FAMILY_UNIX
    -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS
    -D_LIBUNWIND_IS_NATIVE_ONLY
)

SRCS(
    src/AcceptCertificateHandler.cpp
    src/CertificateHandlerFactory.cpp
    src/CertificateHandlerFactoryMgr.cpp
    src/Context.cpp
    src/HTTPSClientSession.cpp
    src/HTTPSSessionInstantiator.cpp
    src/HTTPSStreamFactory.cpp
    src/InvalidCertificateHandler.cpp
    src/KeyConsoleHandler.cpp
    src/KeyFileHandler.cpp
    src/PrivateKeyFactory.cpp
    src/PrivateKeyFactoryMgr.cpp
    src/PrivateKeyPassphraseHandler.cpp
    src/RejectCertificateHandler.cpp
    src/SSLException.cpp
    src/SSLManager.cpp
    src/SecureSMTPClientSession.cpp
    src/SecureServerSocket.cpp
    src/SecureServerSocketImpl.cpp
    src/SecureSocketImpl.cpp
    src/SecureStreamSocket.cpp
    src/SecureStreamSocketImpl.cpp
    src/Session.cpp
    src/Utility.cpp
    src/VerificationErrorArgs.cpp
    src/X509Certificate.cpp
)

END()
