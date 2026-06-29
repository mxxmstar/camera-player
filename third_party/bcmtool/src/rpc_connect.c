#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "rpc_connect.h"

/**
    @name RPC Connection Design ID
    @{
    @brief RPC Connection Design ID's
*/
#define BRCM_SWDSGN_RPC_CONTEXT_MAGIC_MACRO (0xB100U)  /**< @brief #RPC_CONTEXT_MAGIC   */
#define BRCM_SWDSGN_RPC_CONTEXT_TYPE        (0xB101U)  /**< @brief #RPC_ContextType     */
#define BRCM_SWDSGN_RPC_OPEN_PROC           (0xB108U)  /**< @brief #RPC_Open            */
#define BRCM_SWDSGN_RPC_SEND_PROC           (0xB109U)  /**< @brief #RPC_Send            */
#define BRCM_SWDSGN_RPC_RECV_PROC           (0xB10AU)  /**< @brief #RPC_Recv            */
#define BRCM_SWDSGN_RPC_SENDRECV_PROC       (0xB10BU)  /**< @brief #RPC_SendRecv        */
#define BRCM_SWDSGN_RPC_CLOSE_PROC          (0xB10CU)  /**< @brief #RPC_Close           */
/** @} */


/**
    @brief RPC Session Context

    @trace #BRCM_SWARCH_RPC_OPEN_PROC
    @trace #BRCM_SWARCH_RPC_SEND_PROC
    @trace #BRCM_SWARCH_RPC_RECV_PROC
    @trace #BRCM_SWARCH_RPC_CLOSE_PROC
    @trace #BRCM_SWREQ_RPC
*/
#define RPC_CONTEXT_MAGIC (0xA55AA55AUL)

/**
    @brief RPC Session Context

    @trace #BRCM_SWARCH_RPC_OPEN_PROC
    @trace #BRCM_SWARCH_RPC_SEND_PROC
    @trace #BRCM_SWARCH_RPC_RECV_PROC
    @trace #BRCM_SWARCH_RPC_CLOSE_PROC
    @trace #BRCM_SWREQ_RPC
*/
typedef struct sRPC_ContextType {
    uint32_t        magic;      /**< @brief Magic, must be #RPC_CONTEXT_MAGIC   */
    SOCKET          fd;         /**< @brief File descriptor for socket in linux */
    RPC_MsgVerType  version;    /**< @brief Message Version                     */
    uint32_t        timeout;    /**< @brief timeout in milliseconds for commands
                                            to fail */
    uint32_t        filledSize; /**< @brief Message filled size */
} RPC_ContextType;

/**
    @brief Connect to the Communication Interface
*/
int32_t RPC_Open(const char* aSockName, uint16_t aPort,
                    uint32_t aTimeoutMs, BCM_HandleType *aHdl)
{
    if ((NULL == aSockName) || (NULL == aHdl) || (0U == aPort)) {
        return BCM_ERR_INVAL_PARAMS;
    }

    int32_t retVal;
    int32_t sysRet = -1;

    RPC_ContextType *ctx= (RPC_ContextType*)malloc(sizeof(RPC_ContextType));
    if (ctx == NULL) {
        return BCM_ERR_NOMEM;
    }
    BCM_MemSet(ctx, 0, sizeof(RPC_ContextType));

    ctx->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ctx->fd == INVALID_SOCKET) {
        free(ctx);
        return BCM_ERR_NOPERM;
    }

    struct sockaddr_in net = { 0 };
    net.sin_family = AF_INET;
    net.sin_port = htons(aPort);
    inet_pton(AF_INET, aSockName, &net.sin_addr);
    sysRet = connect(ctx->fd, (struct sockaddr *)&net, sizeof(struct sockaddr_in));
    if (sysRet != SOCKET_ERROR) {
        ctx->magic = RPC_CONTEXT_MAGIC;
        ctx->timeout = aTimeoutMs;
        *aHdl = (BCM_HandleType)ctx;
        return BCM_ERR_OK;
    }

    closesocket(ctx->fd);
    free(ctx);

    return BCM_ERR_TIME_OUT;
}

/**
    @brief Connect to the Communication Interface
*/
int32_t RPC_Send(BCM_HandleType aHdl,
                     BCM_MsgType aCmd,
                     const uint8_t *aMsg,
                     uint32_t aSize)
{
    int32_t ret = BCM_ERR_OK;

    if ((0ULL == aHdl) || ((NULL == aMsg) && (0UL != aSize))) {
        ret = BCM_ERR_INVAL_PARAMS;
    } else {
        RPC_ContextType *ctx = (RPC_ContextType *)aHdl;
        if (RPC_CONTEXT_MAGIC != ctx->magic) {
            ret = BCM_ERR_INVAL_PARAMS;
        } else {
            int sysRet;
            RPC_MsgType msg = {0};
            msg.magic = CPU_NativeToLE32(RPC_MSG_MAGIC);
            msg.version = CPU_NativeToLE32(RPC_MSG_VER_1_0);
            msg.cmd = CPU_NativeToLE32(aCmd);
            msg.len = CPU_NativeToLE32(aSize);
            if ((NULL != aMsg) && (0UL != aSize)) {
                BCM_MemCpy(msg.payload, aMsg, aSize);
            }
            sysRet = send(ctx->fd, (const char*)&msg, sizeof(msg), 0);
            if (sysRet >= 0) {
                ret = BCM_ERR_OK;
            } else if ((errno == ENOBUFS) || (errno == ENOMEM))  {
                ret = BCM_ERR_NOMEM;
            } else {
                ret = BCM_ERR_NODEV;
            }
        }
    }

    return ret;
}

/**
    @brief Connect to the Communication Interface

    @param[in] outStr for debug.
    example:
        outStr.append("debug info")
        outStr = QString("debug info")
*/
int32_t RPC_Recv(BCM_HandleType aHdl, uint8_t * const aMsg, uint32_t * const aSize)
{
    int32_t retVal = BCM_ERR_INVAL_PARAMS;

    if ((0ULL == aHdl) || (NULL == aMsg) || (NULL == aSize)) {
        retVal = BCM_ERR_INVAL_PARAMS;
    } else {
        RPC_ContextType *ctx = (RPC_ContextType *)aHdl;
        uint32_t bufLen = 0;
        RPC_MsgType tmpMsg = {0};
        if (RPC_CONTEXT_MAGIC != ctx->magic) {
            retVal = BCM_ERR_INVAL_PARAMS;
        } else {
            do {
                bufLen = sizeof(RPC_MsgType) - ctx->filledSize;
                int sysRet = recv(ctx->fd, (char *)&tmpMsg + ctx->filledSize, bufLen, 0);
                if (sysRet > 0) {
                    ctx->filledSize += sysRet;
                    if (RPC_MSG_HEADER_SIZE > ctx->filledSize) {
                        /* Entire header not received */
                        retVal = BCM_ERR_EAGAIN;
                    } else if (RPC_MSG_MAGIC != tmpMsg.magic) {
                        /* Discard this data and wait for valid message */
                        ctx->filledSize = 0UL;
                        retVal = BCM_ERR_INVAL_MAGIC;
                    } else if (ctx->filledSize < sizeof(RPC_MsgType)) {
                        /* Wait till payload gets received */
                        retVal = BCM_ERR_EAGAIN;
                    } else {
                        ctx->filledSize = 0UL;
                        *aSize = CPU_LEToNative32(tmpMsg.len);
                        retVal = CPU_LEToNative32(tmpMsg.response);
                        BCM_MemCpy(aMsg, &tmpMsg.payload, *aSize);
                    }
                } else if (sysRet == 0) {
                    if (ctx->filledSize != 0) {
                        retVal = BCM_ERR_EAGAIN;
                    } else {
                        retVal = BCM_ERR_NOMEM;
                    }
                } else {
                    retVal = BCM_ERR_NODEV;
                }
            } while (BCM_ERR_EAGAIN == retVal);
        }
    }

    return retVal;
}

/**
    @brief Connect to the Communication Interface
*/
int32_t RPC_SendRecv (BCM_HandleType aHdl,
                         BCM_MsgType aCmd,
                         const uint8_t *aInMsg,
                         uint32_t aInLen,
                         uint8_t * const aOutMsg,
                         uint32_t * const aOutLen)
{
    int32_t retVal;

    retVal = RPC_Send(aHdl, aCmd, aInMsg, aInLen);
    if (BCM_ERR_OK == retVal) {
        /* memset Before Sending the Response.*/
        if (NULL != aOutMsg) {
            BCM_MemSet((uint8_t *)aOutMsg, 0, aInLen);
        }
        retVal = RPC_Recv(aHdl, aOutMsg, aOutLen);
    }

    return retVal;
}

/**
    @brief Connect to the Communication Interface

    @trace #BRCM_SWARCH_RPC_CLOSE_PROC
    @trace #BRCM_SWREQ_RPC

    @code{.unparsed}
    ctx = (RPC_ContextType *)aHdl
    if ctx is NULL
        retVal = BCM_ERR_INVAL_PARAMS;
    else
        if ctx->magic is not RPC_CONTEXT_MAGIC
            retVal = BCM_ERR_INVAL_PARAMS;
        else
            if ctx->fd > 0
                close(ctx->fd)
            free(ctx);
            retVal = BCM_ERR_OK;
    return retVal;
    @endcode
*/
int32_t RPC_Close(BCM_HandleType aHdl)
{
    int32_t retVal = BCM_ERR_OK;
    RPC_ContextType *ctx = (RPC_ContextType *)aHdl;

    if (NULL == ctx) {
        retVal = BCM_ERR_INVAL_PARAMS;
    } else {
        if (RPC_CONTEXT_MAGIC != ctx->magic) {
            retVal = BCM_ERR_INVAL_PARAMS;
        } else {
            if (ctx->fd > 0) {
                closesocket(ctx->fd);
            }
            free(ctx);
            retVal = BCM_ERR_OK;
        }
    }

    return retVal;
}
/** @} */
