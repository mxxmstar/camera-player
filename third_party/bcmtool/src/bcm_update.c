#include "bcm_update.h"
#include "rpc_connect.h"


/**

    @trace #BRCM_SWARCH_UPDATE_HEALTH_CHECK_PROC
    @trace #BRCM_SWREQ_UPDATE

    @code{.unparsed}
    @endcode
*/
int32_t UPDATE_HealthCheck(BCM_HandleType aConnHdl, PTBL_IdType aPid, IMGL_VersionType *aVersion)
{
    int32_t retVal;

    if (NULL == aVersion) {
        retVal = BCM_ERR_INVAL_PARAMS;
    } else {
        uint32_t respLen = sizeof(RPC_MsgType);
        UPDATE_HealthCheckMsgType healthCheck;
        BCM_MemSet((uint8_t *)&healthCheck, 0, sizeof(UPDATE_HealthCheckMsgType));

        healthCheck.pid = CPU_NativeToLE16(aPid);
        retVal = RPC_SendRecv(aConnHdl, UPDATE_ID_HEALTH_CHECK, (const uint8_t *)&healthCheck,
                                      sizeof(UPDATE_HealthCheckMsgType),
                                      (uint8_t * const)&healthCheck, &respLen);
        if(BCM_ERR_OK == retVal) {
            BCM_MemCpy((uint8_t *)aVersion, (uint8_t *)&healthCheck.version, sizeof(IMGL_VersionType));
            aVersion->magic = CPU_LEToNative32(healthCheck.version.magic);
            aVersion->major = CPU_LEToNative32(healthCheck.version.major);
            aVersion->minor = CPU_LEToNative32(healthCheck.version.minor);
        }
    }

    return retVal;
}

/**

    @trace #BRCM_SWARCH_UPDATE_SAFE_INSTALL_PROC
    @trace #BRCM_SWARCH_UPDATE_FULL_INSTALL_PROC
    @trace #BRCM_SWARCH_UPDATE_RAW_INSTALL_PROC
    @trace #BRCM_SWREQ_UPDATE

    @code{.unparsed}
    @endcode
*/
uint32_t UPDATE_InstallHost(BCM_HandleType aConnHdl, UPDATE_InstallCfgMsgType *aInstallMsg,
    uint32_t *aRecvFileSize, BCM_MsgType aCmd)
{
    int32_t retVal;

    uint32_t respLen = sizeof(RPC_MsgType);
    UPDATE_InstallMsgType install;
    BCM_MemSet((uint8_t *)&install, 0, sizeof(UPDATE_InstallMsgType));

    install.cfg.nvmChannel = CPU_NativeToLE32(aInstallMsg->nvmChannel);
    install.cfg.fetchChannel = CPU_NativeToLE32(aInstallMsg->fetchChannel);
    install.cfg.nvmEraseSize = CPU_NativeToLE32(aInstallMsg->nvmEraseSize);
    install.cfg.fileSize = CPU_NativeToLE32(aInstallMsg->fileSize);
    install.cfg.ipAddr = CPU_NativeToLE32(aInstallMsg->ipAddr);
    install.cfg.portNum = CPU_NativeToLE32(aInstallMsg->portNum);
    install.recvFileSize = 0UL;
    BCM_MemCpy(&install.cfg.name[0UL], &aInstallMsg->name[0UL], UPDATE_MAX_FILENAME);
    retVal = RPC_SendRecv(aConnHdl, aCmd, (const uint8_t *)&install,
                                  sizeof(UPDATE_InstallMsgType),
                                  (uint8_t * const)&install, &respLen);
    if(BCM_ERR_OK == retVal) {
        *aRecvFileSize = CPU_LEToNative32(install.recvFileSize);
    }

    return retVal;
}

/**

    @trace #BRCM_SWARCH_UPDATE_FULL_INSTALL_PROC
    @trace #BRCM_SWREQ_UPDATE

    @code{.unparsed}
    @endcode
*/
int32_t UPDATE_FullInstall(BCM_HandleType aConnHdl, UPDATE_InstallCfgMsgType *aInstallMsg,
    uint32_t *aRecvFileSize)
{
    int32_t retVal;

    if ((NULL == aInstallMsg) ||
        (NULL == aRecvFileSize)) {
        retVal = BCM_ERR_INVAL_PARAMS;
    } else {
        retVal = UPDATE_InstallHost(aConnHdl, aInstallMsg, aRecvFileSize, UPDATE_ID_FULL_INSTALL);
    }

    return retVal;
}
