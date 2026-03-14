#pragma once

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#ifdef GOGGLES_DIAGNOSTICS_FORENSIC

#define GOGGLES_DIAG_FORENSIC_SCOPE(name) (void)(name)
#define GOGGLES_DIAG_FORENSIC_CAPTURE(session, pass, cmd)                                          \
    do {                                                                                           \
        (void)(session);                                                                           \
        (void)(pass);                                                                              \
        (void)(cmd);                                                                               \
    } while (0)

#else

#define GOGGLES_DIAG_FORENSIC_SCOPE(name) (void)0
#define GOGGLES_DIAG_FORENSIC_CAPTURE(session, pass, cmd) (void)0

#endif

// NOLINTEND(cppcoreguidelines-macro-usage)
