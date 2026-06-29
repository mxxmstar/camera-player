#include <stdio.h>
#include <string.h>
#include "bcm_config.h"
#include "rpc_connect.h"

uint32_t CONFIG_ExtractItem(uint8_t *ctx, char *name, char *val)
{
    uint16_t id;
    uint32_t mask;
    uint32_t len;

    mask = ByteToU32(ctx);
    len = (mask >> 24) & 0xFF;
    id = ((mask >> 16) & 0xFF) | (((mask >> 8) & 0xFF) << 8);

    switch (id) {
        case CONFIG_MEDIA_MIRROR:
            strcpy(name, "mirror mode");
            sprintf(val, "%d", ctx[4]);
            break;
        case CONFIG_MEDIA_FPS:
            strcpy(name, "FPS");
            sprintf(val, "%d", ctx[4]);
            break;
        case CONFIG_NETWORK_DHCP:
            strcpy(name, "DHCP");
            sprintf(val, "%d", ctx[4]);
            break;
        case CONFIG_MEDIA_SOMEIPUDPPORT:
            strcpy(name, "SOMEIP UDP port");
            sprintf(val, "%d", (ctx[4] << 8 | ctx[5]));
            break;
        case CONFIG_MEDIA_SOMEIPRTPPORT:
            strcpy(name, "SOMEIP RTP port");
            sprintf(val, "%d", (ctx[4] << 8 | ctx[5]));
            break;
        case CONFIG_NETWORK_IP:
            strcpy(name, "IP");
            strncpy(val, (const char*)&ctx[4], 16);
            break;
        case CONFIG_NETWORK_MAC:
            strcpy(name, "MAC");
            strncpy(val, (const char*)&ctx[4], 18);
            break;
        default:
            len = 0;
            break;
    }

    return 4 + len;
}

int32_t CONFIG_RpcRead(BCM_HandleType aConnHdl, CONFIG_RpcMsg *readMsg)
{
    int32_t retVal;

    retVal = RPC_SendRecv(aConnHdl, CONFIG_CMD_RPC_READ, (const uint8_t *)readMsg,
                                    sizeof(CONFIG_RpcMsg),
                                    (uint8_t * const)readMsg->ctx, &readMsg->len);

    return retVal;
}

int32_t CONFIG_RpcWrite(BCM_HandleType aConnHdl, CONFIG_RpcMsg *writeMsg)
{
    int32_t retVal;

    retVal = RPC_SendRecv(aConnHdl, CONFIG_CMD_RPC_WRITE, (const uint8_t *)writeMsg,
                                    sizeof(CONFIG_RpcMsg),
                                    (uint8_t * const)writeMsg->ctx, &writeMsg->len);
    
    return retVal;
}
