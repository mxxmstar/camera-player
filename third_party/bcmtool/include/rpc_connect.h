#ifndef RPC_CONNECT_H
#define RPC_CONNECT_H

#include <stdint.h>

#include "bcm_common.h"

/**
    @name RPC Message Version Type
    @{
    @brief RPC Message Version Type

    @trace #BRCM_SWREQ_RPC
*/
typedef uint32_t RPC_MsgVerType;            /**< @brief RPC Message Version Type    */
#define RPC_MSG_VER_MINOR_MASK  (0xFFUL)    /**< @brief Mask for Index in the pool  */
#define RPC_MSG_VER_MINOR_SHIFT (0UL)       /**< @brief Shift for Index in the pool */
#define RPC_MSG_VER_MAJOR_MASK  (0xFF00UL)  /**< @brief Mask for Index in the pool  */
#define RPC_MSG_VER_MAJOR_SHIFT (8UL)       /**< @brief Shift for Index in the pool */
#define RPC_MSG_VER_LEGACY      (0UL)       /**< @brief Legacy message type         */
#define RPC_MSG_VER_1_0         (0x0100UL)  /**< @brief Version 1.0                 */
/** @} */

/**
    @brief Magic for RPC_MsgType

    @trace #BRCM_SWREQ_RPC
*/
#define RPC_MSG_MAGIC        (0x5250434DUL)

/**
    @brief RPC Message Type

    'appinfo' stores the received application magic
    and use the same when it send the response.

    @todo Temporary name to avoid conflict with existing code.
    To be changed to RPC_MsgType later

    @trace #BRCM_SWREQ_RPC
*/
typedef struct sRPC_MsgType {
    uint32_t        magic;          /**< @brief magic holding RPCM ASCII    */
    RPC_MsgVerType  version;        /**< @brief Version                     */
    BCM_MsgType     cmd;            /**< @brief Command ID                  */
    uint32_t        timeoutMs;      /**< @brief Timeout value in ms         */
    int32_t         response;       /**< @brief Response Code               */
    uint32_t        len;            /**< @brief Command payload length      */
    uint32_t        appInfoTop;     /**< @brief appinfo top position        */
    uint32_t        appInfo[RPC_MAX_CNT_APPINFO];
                                    /**< @brief Application Information     */
    uint32_t        rsvd;           /**< @brief Reserved filed used in RPC  */
    uint8_t         payload[RPC_MSG_PAYLOAD_SZ];  /**< @brief Payload */
} RPC_MsgType;

/**
    @brief Size of Message header

    @trace #BRCM_SWREQ_RPC
*/
#define RPC_MSG_HEADER_SIZE      (sizeof(RPC_MsgType) - RPC_MSG_PAYLOAD_SZ)


int32_t RPC_Open(const char *aSockName, uint16_t aPort, uint32_t aTimeoutMs, BCM_HandleType *aHdl);

int32_t RPC_Send(BCM_HandleType aHdl, BCM_MsgType aCmd, const uint8_t *aMsg, uint32_t aSize);

int32_t RPC_Recv(BCM_HandleType aHdl, uint8_t * const aMsg, uint32_t * const aSize);

int32_t RPC_SendRecv(BCM_HandleType aHdl, BCM_MsgType aCmd, const uint8_t *aInMsg, uint32_t aInLen,
    uint8_t * const aOutMsg, uint32_t * const aOutLen);

int32_t RPC_Close(BCM_HandleType aHdl);

#endif // RPC_CONNECT_H
