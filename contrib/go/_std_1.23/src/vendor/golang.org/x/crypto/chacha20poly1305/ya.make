GO_LIBRARY()
IF (OS_DARWIN AND ARCH_ARM64 AND RACE AND CGO_ENABLED OR OS_DARWIN AND ARCH_ARM64 AND RACE AND NOT CGO_ENABLED OR OS_DARWIN AND ARCH_ARM64 AND NOT RACE AND CGO_ENABLED OR OS_DARWIN AND ARCH_ARM64 AND NOT RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_AARCH64 AND RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_AARCH64 AND RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_AARCH64 AND NOT RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_AARCH64 AND NOT RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_ARM6 AND RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_ARM6 AND RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_ARM6 AND NOT RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_ARM6 AND NOT RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_ARM7 AND RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_ARM7 AND RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_ARM7 AND NOT RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_ARM7 AND NOT RACE AND NOT CGO_ENABLED)
    SRCS(
        chacha20poly1305.go
        chacha20poly1305_generic.go
        chacha20poly1305_noasm.go
        xchacha20poly1305.go
    )
ELSEIF (OS_DARWIN AND ARCH_X86_64 AND RACE AND CGO_ENABLED OR OS_DARWIN AND ARCH_X86_64 AND RACE AND NOT CGO_ENABLED OR OS_DARWIN AND ARCH_X86_64 AND NOT RACE AND CGO_ENABLED OR OS_DARWIN AND ARCH_X86_64 AND NOT RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_X86_64 AND RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_X86_64 AND RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_X86_64 AND NOT RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_X86_64 AND NOT RACE AND NOT CGO_ENABLED OR OS_WINDOWS AND ARCH_X86_64 AND RACE AND CGO_ENABLED OR OS_WINDOWS AND ARCH_X86_64 AND RACE AND NOT CGO_ENABLED OR OS_WINDOWS AND ARCH_X86_64 AND NOT RACE AND CGO_ENABLED OR OS_WINDOWS AND ARCH_X86_64 AND NOT RACE AND NOT CGO_ENABLED)
    SRCS(
        chacha20poly1305.go
        chacha20poly1305_amd64.go
        chacha20poly1305_amd64.s
        chacha20poly1305_generic.go
        xchacha20poly1305.go
    )
ENDIF()
END()
