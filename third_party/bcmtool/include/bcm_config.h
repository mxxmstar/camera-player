#ifndef BCM_CONFIG_H
#define BCM_CONFIG_H

#include <stdint.h>
#include <stdlib.h>

#include "bcm_common.h"

/**
    @brief Macro to Construct Flash CmdID

    @trace #BRCM_SWREQ_FLASH
*/
#define CONFIG_ID_OF(aId)     \
    BCM_MSG(BCM_GROUPID_NVM, BCM_CFG_ID, aId)

/**
  @name Flash module supported command
  @{
  @brief Flash module supported command

  @trace #BRCM_SWREQ_FLASH
*/
typedef uint32_t CONFIG_CmdIDType;            /**< @brief typedef for flash cmdID */
#define CONFIG_CMD_RPC_READ                   CONFIG_ID_OF(1UL)
#define CONFIG_CMD_RPC_WRITE                  CONFIG_ID_OF(2UL)


typedef uint16_t CONFIG_ItemIDType;
#define CONFIG_MEDIA_MIRROR                   0x0101
#define CONFIG_MEDIA_FPS                      0x0102
#define CONFIG_MEDIA_SOMEIPUDPPORT            0x0103
#define CONFIG_MEDIA_SOMEIPRTPPORT            0x0104

#define CONFIG_NETWORK_DHCP                   0x0201
#define CONFIG_NETWORK_IP                     0x0202
#define CONFIG_NETWORK_MAC                    0x0203
                                        
#define CONFIG_ITEM_HEADER_R(id, len)    (uint32_t)((0xAB) | ((id >> 8) & 0xFF) << 8 | (id & 0xFF) << 16 | (len & 0xFF) << 24)

typedef struct sCONFIG_RpcMsgType {
    uint8_t             ctx[256UL];  
    uint32_t            len;                                                    
} CONFIG_RpcMsg;

extern int32_t CONFIG_RpcRead(BCM_HandleType aConnHdl, CONFIG_RpcMsg *readMsg);

extern uint32_t CONFIG_ExtractItem(uint8_t *ctx, char *name, char *val);

extern int32_t CONFIG_RpcWrite(BCM_HandleType aConnHdl, CONFIG_RpcMsg *writeMsg);

#endif // BCM_CONFIG_H