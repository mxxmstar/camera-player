#include <stdio.h>

#include "bcm_dmon.h"
#include "rpc_connect.h"

/**
    @name Device Monitor Host Helper Design IDs
    @{
    @brief Device Monitor Host Design IDs
*/
#define BRCM_SWDSGN_DMON_READMEM_PROC                      (0xBD01U)   /**< @brief #DMON_ReadMem                   */
#define BRCM_SWDSGN_DMON_WRITEMEM_PROC                     (0xBD02U)   /**< @brief #DMON_WriteMem                  */
#define BRCM_SWDSGN_DMON_PING_PROC                         (0xBD03U)   /**< @brief #DMON_Ping                      */
#define BRCM_SWDSGN_DMON_SYNC_PROC                         (0xBD04U)   /**< @brief #DMON_Sync                      */
#define BRCM_SWDSGN_DMON_GETSWVERSION_PROC                 (0xBD05U)   /**< @brief #DMON_GetSwVersion              */
#define BRCM_SWDSGN_DMON_GETHWVERSION_PROC                 (0xBD06U)   /**< @brief #DMON_GetHwVersion              */
#define BRCM_SWDSGN_DMON_REBOOT_PROC                       (0xBD07U)   /**< @brief #DMON_Reboot                    */
#define BRCM_SWDSGN_DMON_SYNC_WAIT_PROC                    (0xBD08U)   /**< @brief #DMON_SyncWait                  */
#define BRCM_SWDSGN_DMON_DEEPSLEEP_PROC                    (0xBD09U)   /**< @brief #DMON_DeepSleep                   */
/** @} */

/** @brief Read Data from  Memory

    @trace #BRCM_SWARCH_DMON_READMEM_PROC
    @trace #BRCM_SWREQ_DMON

    @code{.unparsed}
    @endcode
*/
int32_t DMON_ReadMem(BCM_HandleType aHdl,
                        uint32_t        aAddr,
                        uint32_t        aWidth,
                        DMON_DeviceType aDeviceID,
                        uint32_t        *aData)
{
    uint32_t data;
    DMON_MemAccessMsgType readMem = {0};
    uint32_t respLen = sizeof(DMON_MemAccessMsgType);
    int32_t retVal;

    if((NULL == aData) ||
        ((aWidth != 8UL) && (aWidth != 16UL) && (aWidth != 32UL))){
        retVal = BCM_ERR_INVAL_PARAMS;
    } else {
        readMem.addr     = CPU_NativeToLE32(aAddr);
        readMem.width    = CPU_NativeToLE32(aWidth);
        readMem.len      = CPU_NativeToLE32(1UL);
        readMem.deviceID = CPU_NativeToLE32(aDeviceID);
        retVal = RPC_SendRecv(aHdl, DMON_ID_MEM_READ, (const uint8_t *)&readMem,
                                      sizeof(DMON_MemAccessMsgType),
                                      (uint8_t * const)&readMem, &respLen);
        if(BCM_ERR_OK == retVal) {
            BCM_MemCpy(&data, &readMem.data,sizeof(data));
            *aData = CPU_LEToNative32(data);
        }
    }

    return retVal;
}

/** @brief Write Data to  Memory

    @trace #BRCM_SWARCH_DMON_WRITEMEM_PROC
    @trace #BRCM_SWREQ_DMON

    @code{.unparsed}
    @endcode
*/
int32_t DMON_WriteMem(BCM_HandleType aHdl,
                        uint32_t        aAddr,
                        uint32_t        aWidth,
                        DMON_DeviceType aDeviceID,
                        uint32_t        aData)
{
    uint32_t data;
    DMON_MemAccessMsgType writeMem = {0};
    uint32_t respLen = sizeof(DMON_MemAccessMsgType);
    int32_t retVal;

    if((aWidth != 8UL) && (aWidth != 16UL) && (aWidth != 32UL)){
        retVal = BCM_ERR_INVAL_PARAMS;
    } else {
        writeMem.addr     = CPU_NativeToLE32(aAddr);
        writeMem.width    = CPU_NativeToLE32(aWidth);
        writeMem.len      = CPU_NativeToLE32(1UL);
        writeMem.deviceID = CPU_NativeToLE32(aDeviceID);
        data              = CPU_NativeToLE32(aData);
        BCM_MemCpy(&writeMem.data, &data, sizeof(data));
        retVal = RPC_SendRecv(aHdl, DMON_ID_MEM_WRITE, (const uint8_t *)&writeMem,
                                       sizeof(DMON_MemAccessMsgType),
                                       (uint8_t * const)&writeMem, &respLen);
    }

    return retVal;
}

/** @brief Ping Message

    @trace #BRCM_SWARCH_DMON_PING_PROC
    @trace #BRCM_SWREQ_DMON

    @code{.unparsed}
    @endcode
*/
int32_t DMON_Ping(BCM_HandleType aHdl,
                     DMON_PingMsgType *aPing)
{
    DMON_PingMsgType pingMsg = {0};
    uint32_t respLen = sizeof(DMON_PingMsgType);
    int32_t retVal = BCM_ERR_INVAL_PARAMS;

    if(NULL != aPing) {
        retVal = RPC_SendRecv(aHdl, DMON_ID_PING, (const uint8_t *)&pingMsg, 0UL,
                                 (uint8_t * const)&pingMsg, &respLen);
        if(BCM_ERR_OK == retVal) {
            aPing->mode  = CPU_LEToNative32(pingMsg.mode);
            aPing->version.manuf    = CPU_LEToNative32(pingMsg.version.manuf);
            aPing->version.model    = CPU_LEToNative32(pingMsg.version.model);
            aPing->version.rev      = CPU_LEToNative32(pingMsg.version.rev);
            aPing->version.secMode  = CPU_LEToNative32(pingMsg.version.secMode);

        }
    }

    return retVal;
}

/** @brief Sync Message

    @trace #BRCM_SWARCH_DMON_SYNC_PROC
    @trace #BRCM_SWREQ_DMON

    @code{.unparsed}
    @endcode
*/
int32_t DMON_Sync(BCM_HandleType aHdl,
                    DMON_SyncMsgType *aSync)
{
    DMON_SyncMsgType syncMsg = {0};
    uint32_t respLen = sizeof(DMON_SyncMsgType);
    int32_t retVal = BCM_ERR_INVAL_PARAMS;

    if(NULL != aSync) {
        retVal = RPC_SendRecv(aHdl, DMON_ID_SYNC, (const uint8_t *)&syncMsg, 0UL,
                                           (uint8_t * const)&syncMsg, &respLen);
        if(BCM_ERR_OK == retVal) {
            aSync->mode  = CPU_LEToNative32(syncMsg.mode);
            aSync->version.manuf    = CPU_LEToNative32(syncMsg.version.manuf);
            aSync->version.model    = CPU_LEToNative32(syncMsg.version.model);
            aSync->version.rev      = CPU_LEToNative32(syncMsg.version.rev);
            aSync->version.secMode  = CPU_LEToNative32(syncMsg.version.secMode);
            aSync->state            = CPU_LEToNative32(syncMsg.state);
            aSync->initTime         = CPU_LEToNative64(syncMsg.initTime);
            aSync->readyTime        = CPU_LEToNative64(syncMsg.readyTime);
            aSync->runTime          = CPU_LEToNative64(syncMsg.runTime);
        }
    }

    return retVal;
}

/**

    @trace #BRCM_SWARCH_DMON_SYNC_WAIT_PROC
    @trace #BRCM_SWREQ_DMON

    @code{.unparsed}
    syncMsg.state = CPU_LEToNative32(aState);
    retVal = RPC_SendRecv(aHdl, DMON_ID_WAIT, &syncMsg, sizeof(DMON_SyncMsgType),
                                       &syncMsg, &respLen);
    if retVal is OK
        Convert endianness and Copy syncMsg to aSync
    @endcode
*/
int32_t DMON_SyncWait(BCM_HandleType aHdl, BCM_StateType aState,
                    DMON_SyncMsgType *aSync)
{
    DMON_SyncMsgType syncMsg = {0};
    uint32_t respLen = sizeof(DMON_SyncMsgType);
    int32_t retVal = BCM_ERR_INVAL_PARAMS;

    if(NULL != aSync) {
        syncMsg.state = CPU_LEToNative32(aState);
        retVal = RPC_SendRecv(aHdl, DMON_ID_SYNC_WAIT, (const uint8_t *)&syncMsg, sizeof(DMON_SyncMsgType),
                                           (uint8_t * const)&syncMsg, &respLen);
        if(BCM_ERR_OK == retVal) {
            aSync->mode  = CPU_LEToNative32(syncMsg.mode);
            aSync->version.manuf    = CPU_LEToNative32(syncMsg.version.manuf);
            aSync->version.model    = CPU_LEToNative32(syncMsg.version.model);
            aSync->version.rev      = CPU_LEToNative32(syncMsg.version.rev);
            aSync->version.secMode  = CPU_LEToNative32(syncMsg.version.secMode);
            aSync->state            = CPU_LEToNative32(syncMsg.state);
            aSync->initTime         = CPU_LEToNative64(syncMsg.initTime);
            aSync->readyTime        = CPU_LEToNative64(syncMsg.readyTime);
            aSync->runTime          = CPU_LEToNative64(syncMsg.runTime);
        }
    }

    return retVal;
}

/** @brief Get Software Version

    @trace #BRCM_SWARCH_DMON_GETSWVERSION_PROC
    @trace #BRCM_SWREQ_DMON

    @code{.unparsed}
    @endcode
*/
int32_t DMON_GetSwVersion(BCM_HandleType aHdl,
                            DMON_SwVersionMsgType *aSwVersion)
{
    uint32_t respLen = sizeof(DMON_SwVersionMsgType);
    int32_t retVal = BCM_ERR_INVAL_PARAMS;

    if(NULL != aSwVersion) {
        retVal = RPC_SendRecv(aHdl, DMON_ID_SW_VERSION, (const uint8_t *)aSwVersion, 0UL,
                                           (uint8_t * const)aSwVersion, &respLen);
    }

    return retVal;
}

/** @brief Get Hardware version

    @trace #BRCM_SWARCH_DMON_GETHWVERSION_PROC
    @trace #BRCM_SWREQ_DMON

    @code{.unparsed}
    @endcode
*/
int32_t DMON_GetHwVersion(BCM_HandleType aHdl,
                            DMON_HwVersionMsgType *aHwVersion)
{
    DMON_HwVersionMsgType hwVersion = {0};
    uint32_t respLen = sizeof(DMON_HwVersionMsgType);
    int32_t retVal = BCM_ERR_INVAL_PARAMS;

    if(NULL != aHwVersion) {
        retVal = RPC_SendRecv(aHdl, DMON_ID_HW_VERSION, (const uint8_t *)&hwVersion, 0UL,
                                           (uint8_t * const)&hwVersion, &respLen);
        if(BCM_ERR_OK == retVal) {
            aHwVersion->manuf    = CPU_LEToNative32(hwVersion.manuf);
            aHwVersion->model    = CPU_LEToNative32(hwVersion.model);
            aHwVersion->rev      = CPU_LEToNative32(hwVersion.rev);
            aHwVersion->secMode  = CPU_LEToNative32(hwVersion.secMode);
        }
    }

    return retVal;
}

/** @brief Reboot the Device

    @trace #BRCM_SWARCH_DMON_REBOOT_PROC
    @trace #BRCM_SWREQ_DMON

    @code{.unparsed}
    @endcode
*/
int32_t DMON_Reboot(BCM_HandleType aHdl)
{
    int32_t retVal;
    uint32_t respLen = sizeof(DMON_RebootMsgType);
    DMON_RebootMsgType reboot = {0};

    reboot.delayMs = CPU_NativeToLE32(10UL);
    retVal = RPC_SendRecv(aHdl, DMON_ID_REBOOT, (const uint8_t *)&reboot,
                                    sizeof(DMON_RebootMsgType),
                                    (uint8_t * const)&reboot, &respLen);

    return retVal;
}

/** @brief Put the device in DeepSleep mode

    @trace #BRCM_SWARCH_DMON_DEEPSLEEP_PROC
    @trace #BRCM_SWREQ_DMON

    @code{.unparsed}
    @endcode
*/
int32_t DMON_DeepSleep(BCM_HandleType aHdl)
{
    int32_t retVal;
    uint32_t respLen = sizeof(DMON_DeepSleepMsgType);
    DMON_DeepSleepMsgType deepSleep = {0};

    deepSleep.delayMs = CPU_NativeToLE32(10UL);
    retVal = RPC_SendRecv(aHdl, DMON_ID_DEEPSLEEP, (const uint8_t *)&deepSleep,
                                    sizeof(DMON_DeepSleepMsgType),
                                    (uint8_t * const)&deepSleep, &respLen);

    return retVal;
}
/** @} */
