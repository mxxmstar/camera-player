#include <stdio.h>
#include <string.h>
#include "bcm_flash.h"
#include "rpc_connect.h"


/** @brief Erase a sector of flash

    @trace #BRCM_SWARCH_FLASH_ERASESECTOR_CMD_PROC
    @trace #BRCM_SWREQ_FLASH
*/
int32_t FLASH_EraseSector(BCM_HandleType aConnHdl,
                               uint32_t aID,
                               uint32_t aAddr)
{
    uint32_t respLen = sizeof(RPC_MsgType);
    int32_t retVal;
    FLASH_RpcEraseInfoType erase;

    BCM_MemSet((uint8_t *)&erase, 0U, sizeof(FLASH_RpcEraseInfoType));
    erase.hwID = CPU_NativeToLE32(aID);
    erase.addr = CPU_NativeToLE32(aAddr);
    erase.len  = CPU_NativeToLE32(FLASH_MAX_SECTOR_SIZE);
    retVal = RPC_SendRecv(aConnHdl, FLASH_CMD_RPC_ERASE, (const uint8_t *)&erase,
                                    sizeof(FLASH_RpcEraseInfoType),
                                    (uint8_t * const)&erase, &respLen);

    return retVal;
}

/** @brief Write to Flash

    @trace #BRCM_SWARCH_FLASH_WRITEPAGE_CMD_PROC
    @trace #BRCM_SWREQ_FLASH
*/
int32_t FLASH_WritePage(BCM_HandleType aConnHdl,
                            uint32_t aID,
                            uint32_t aAddr,
                            const uint8_t *const aBuf)
{
    uint32_t respLen = sizeof(RPC_MsgType);
    int32_t retVal = BCM_ERR_INVAL_PARAMS;
    FLASH_RpcWriteInfoType write;

    if(NULL != aBuf) {
        BCM_MemSet((uint8_t *)&write, 0U, sizeof(FLASH_RpcWriteInfoType));
        write.hwID = CPU_NativeToLE32(aID);
        write.addr = CPU_NativeToLE32(aAddr);
        write.len  = CPU_NativeToLE32(FLASH_MAX_PAGE_SIZE);
        BCM_MemCpy(write.bufIn, aBuf, FLASH_MAX_PAGE_SIZE);
        retVal = RPC_SendRecv(aConnHdl, FLASH_CMD_RPC_WRITE, (const uint8_t *)&write,
                                        sizeof(FLASH_RpcWriteInfoType),
                                        (uint8_t * const)&write, &respLen);
    }

    return retVal;
}

/** @brief Read from Flash

    @trace #BRCM_SWARCH_FLASH_READPAGE_CMD_PROC
    @trace #BRCM_SWREQ_FLASH


*/
int32_t FLASH_ReadPage(BCM_HandleType aConnHdl,
                                uint32_t aID,
                                uint32_t aAddr,
                                uint8_t *const aBuf)
{
    uint32_t respLen = sizeof(RPC_MsgType);
    int32_t retVal = BCM_ERR_INVAL_PARAMS;
    FLASH_RpcReadInfoType read;

    if(NULL != aBuf) {
        BCM_MemSet((uint8_t *)&read, 0U, sizeof(FLASH_RpcReadInfoType));
        read.hwID = CPU_NativeToLE32(aID);
        read.addr = CPU_NativeToLE32(aAddr);
        read.len  = CPU_NativeToLE32(FLASH_MAX_PAGE_SIZE);
        retVal = RPC_SendRecv(aConnHdl, FLASH_CMD_RPC_READ, (const uint8_t *)&read,
                                        sizeof(FLASH_RpcReadInfoType),
                                        (uint8_t * const)&read, &respLen);
        if(BCM_ERR_OK == retVal) {
            BCM_MemCpy(aBuf, read.bufOut, FLASH_MAX_PAGE_SIZE);
        }
    }

    return retVal;
}


int32_t FLASH_ReadConfig(BCM_HandleType aConnHdl,
                                uint32_t aID,
                                uint32_t aAddr,
                                uint8_t *const aBuf)
{
    uint32_t respLen = sizeof(RPC_MsgType);
    int32_t retVal = BCM_ERR_INVAL_PARAMS;
    FLASH_RpcReadInfoType read;

    if(NULL != aBuf) {
        BCM_MemSet((uint8_t *)&read, 0U, sizeof(FLASH_RpcReadInfoType));
        read.hwID = CPU_NativeToLE32(aID);
        read.addr = CPU_NativeToLE32(aAddr);
        read.len  = CPU_NativeToLE32(FLASH_MAX_PAGE_SIZE);
        retVal = RPC_SendRecv(aConnHdl, FLASH_CMD_RPC_READ, (const uint8_t *)&read,
                                        sizeof(FLASH_RpcReadInfoType),
                                        (uint8_t * const)&read, &respLen);
        if(BCM_ERR_OK == retVal) {
            BCM_MemCpy(aBuf, read.bufOut, FLASH_MAX_PAGE_SIZE);
        }
    }

    return retVal;
}

/** @} */
