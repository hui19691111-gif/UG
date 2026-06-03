/*
  Required for native NX plugin signing/resource retention.
*/

#include <uf_defs.h>

static unsigned char nxauthblock[] = "NXAUTHBLOCK      "
                                     "                                                  "
                                     "                                                  "
                                     "                                                  "
                                     "                                                  "
                                     "                                                  "
                                     "                                                  "
                                     "                                                  "
                                     "                                                  "
                                     "                                                  "
                                     "                                                  "
                                     "      NXAUTHBLOCK";

extern "C" DllExport void NXSigningResource(void)
{
    static bool doNothing = false;
    if (doNothing)
    {
        nxauthblock[0] = 'N';
    }
}
