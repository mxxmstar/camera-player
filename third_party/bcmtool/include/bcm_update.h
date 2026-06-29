#ifndef BCM_UPDATE_H
#define BCM_UPDATE_H

#include <stdint.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/stat.h>

#include "bcm_common.h"

/**
    @name Partition IDs

    @anchor PTBL_ID_CONSTRUCT

    @{
    @brief Partition ID

    @trace #BRCM_SWREQ_PTBL_LAYOUT
*/
typedef uint16_t PTBL_IdType;               /**< @brief 16-bit Partition ID     */
#define PTBL_ID_BL              (1U)        /**< @brief Bootloader Partition ID */
#define PTBL_ID_FW              (3U)        /**< @brief Firmware Partition ID   */
#define PTBL_ID_SYSCFG          (5U)        /**< @brief SysCfg Partition ID     */
#define PTBL_ID_CRASH_DUMP      (7U)        /**< @brief Crash Partition ID      */
#define PTBL_ID_SECURE_KEY      (8U)        /**< @brief Secure Key Partition ID */
#define PTBL_ID_USER_DATA       (9U)        /**< @brief User Data Partition ID  */
#define PTBL_ID_TYPE_MASK       (0xFFU)     /**< @brief Partition ID mask       */
#define PTBL_ID_TYPE_SHIFT      (0U)        /**< @brief Partition ID shift      */
#define PTBL_ID_COPY_MASK       (0xF00U)    /**< @brief Partition ID Copy mask  */
#define PTBL_ID_COPY_SHIFT      (8U)        /**< @brief Partition ID Copy shift */
#define PTBL_ID_SKIP_MASK       (0x01U)     /**< @brief Partition ID skip mask  */
#define PTBL_ID_SKIP_SHIFT      (0U)        /**< @brief Partition ID skip shift */
#define PTBL_ID_CONSTRUCT(aPid, aCopyNum)  \
    (((((PTBL_IdType) (aPid)) << (PTBL_ID_TYPE_SHIFT)) & (PTBL_ID_TYPE_MASK)) \
     | ((((PTBL_IdType) (aCopyNum)) << (PTBL_ID_COPY_SHIFT)) & (PTBL_ID_COPY_MASK)))
/** @} */

/**
    @brief Image version macros

    @trace #BRCM_SWREQ_IMGL
*/
#define IMGL_VERSION_MAGIC              (0x56455253UL)

/**
    @brief Image Major Version

    @trace #BRCM_SWREQ_IMGL
*/
typedef uint32_t IMGL_MajorVersionType;

/**
    @brief Image Minor Version

    @trace #BRCM_SWREQ_IMGL
*/
typedef uint32_t IMGL_MinorVersionType;

/**
    @name Build info fields
    @{
    @brief Build information

    Build information field lengths

    @trace #BRCM_SWREQ_IMGL
*/
#define IMGL_VERS_LEN_TAG          (24UL)   /**< @brief Tag string */
#define IMGL_VERS_LEN_OS           (8UL)    /**< @brief OS name */
#define IMGL_VERS_LEN_APP          (20UL)   /**< @brief Application Name */
#define IMGL_VERS_LEN_IMG_TYPE     (4UL)    /**< @brief 'Dev' or 'Rel' */
#define IMGL_VERS_LEN_IMG_VERS     (8UL)    /**< @brief Alpha numeric version */
#define IMGL_VERS_LEN_TIME         (52UL)   /**< @brief date, time, Server name and user name */
/** @} */

/**
    @brief Version type

    Version information

    @trace #BRCM_SWREQ_IMGL
 */
typedef struct sIMGL_VersionType {
    uint32_t              magic;           /**< @brief Magic VERS #IMGL_VERSION_MAGIC */
    IMGL_MajorVersionType major;           /**< @brief Major version number */
    IMGL_MinorVersionType minor;           /**< @brief Minor version number */
    uint8_t               buildInfo[IMGL_VERS_LEN_TAG +
                                    IMGL_VERS_LEN_OS +
                                    IMGL_VERS_LEN_APP +
                                    IMGL_VERS_LEN_IMG_TYPE +
                                    IMGL_VERS_LEN_IMG_VERS +
                                    IMGL_VERS_LEN_TIME];
                                            /**< @brief Build information */
} IMGL_VersionType;

/**
    @brief Health check message

    @trace #BRCM_SWREQ_UPDATE
 */
typedef struct sUPDATE_HealthCheckMsgType {
    PTBL_IdType         pid;    /**< @brief Input: Partition ID to perform health check on (including copy id)*/
    IMGL_VersionType    version;/**< @brief Output: Version of image (image which has the maximum version that appeared first */
} UPDATE_HealthCheckMsgType;

#define BCM_UPD_ID (0x0323U)   /**< @brief Update module  */

/**
    @name UPDATE Version macros
    @{
    @brief Image version macros

    @trace #BRCM_SWREQ_UPDATE
*/
#define UPDATE_ID(x)            BCM_MSG(BCM_GROUPID_NVM, BCM_UPD_ID, (x)) /**< @brief UPDATE ID construction macro */
#define UPDATE_MAGIC            (0x55504454UL)      /**< @brief ASCII "UPDT"        */
#define UPDATE_MAX_FILENAME     (256UL)             /**< @brief Max file name size  */
#define UPDATE_MAX_INFO         (4UL)               /**< @brief Max Info length     */
#define UPDATE_MAX_MSG_SIZE     (448UL)             /**< @brief Max Message size    */

/**
    @name UPDATE Message IDs
    @{
    @brief Message IDs for exchange on message queues and Host

    These are used for interaction over MSGQ interface and RPC commands.
    Hence, this must be within 8-bit space.

    @trace #BRCM_SWREQ_UPDATE
*/
typedef BCM_MsgType UPDATE_IDType;     /**< @brief UPDATE message ID Type */
#define UPDATE_ID_HEALTH_CHECK          UPDATE_ID(0x00U) /**< @brief #UPDATE_HealthCheckMsgType #UPDATE_HealthCheck     */
#define UPDATE_ID_GET_BOOT_COPY_CFG     UPDATE_ID(0x10U) /**< @brief #UPDATE_BootCopyCfgMsgType #UPDATE_GetBootCopyCfg  */
#define UPDATE_ID_SET_BOOT_COPY_CFG     UPDATE_ID(0x11U) /**< @brief #UPDATE_BootCopyCfgMsgType #UPDATE_SetBootCopyCfg  */
#define UPDATE_ID_SAFE_INSTALL          UPDATE_ID(0x20U) /**< @brief #UPDATE_InstallMsgType     #UPDATE_SafeInstall     */
#define UPDATE_ID_FULL_INSTALL          UPDATE_ID(0x21U) /**< @brief #UPDATE_InstallMsgType     #UPDATE_FullInstall     */
#define UPDATE_ID_RAW_INSTALL           UPDATE_ID(0x22U) /**< @brief #UPDATE_InstallMsgType     #UPDATE_RawInstall      */
#define UPDATE_ID_SYNC                  UPDATE_ID(0x30U) /**< @brief #UPDATE_SyncMsgType        #UPDATE_Sync            */
/** @} */

/**
    @name IMGL_ChannelType
    @{
    @brief IMGL Channel ID

    @trace #BRCM_SWREQ_IMGL
*/
typedef uint32_t IMGL_ChannelType;               /**< @brief Loader channel ID */
#define IMGL_CHANNEL_ID_INVALID  (0x00UL)        /**< @brief Invalid           */
#define IMGL_CHANNEL_ID_NVM_0    (0x4E564D30UL)  /**< @brief NVM 0             */
#define IMGL_CHANNEL_ID_NVM_1    (0x4E564D31UL)  /**< @brief NVM 1             */
#define IMGL_CHANNEL_ID_RPC_IPC  (0x52495043UL)  /**< @brief RPC IPC           */
#define IMGL_CHANNEL_ID_RPC_UDP  (0x52554450UL)  /**< @brief RPC UDP           */
#define IMGL_CHANNEL_ID_RPC_FTP  (0x52465450UL)  /**< @brief RPC FTP           */
/** @} */

/**
    @brief Configuration to perform installation of new SW version

    @trace #BRCM_SWREQ_UPDATE
 */
typedef struct sUPDATE_InstallCfgMsgType {
    IMGL_ChannelType    nvmChannel;     /**< @brief Input: NVM Channel to write to                  */
    IMGL_ChannelType    fetchChannel;   /**< @brief Input: Fetch Channel to read from               */
    uint32_t            reserved[2UL];  /**< @brief Input: Reserved field                           */
    uint32_t            nvmEraseSize;   /**< @brief Input: NVM Erase size                           */
    uint32_t            fileSize;       /**< @brief Input: File size                                */
    uint32_t            ipAddr;         /**< @brief Input: IP Address of file server                */
    uint32_t            portNum;        /**< @brief Input: Port number if applicable                */
    uint32_t            info[UPDATE_MAX_INFO];  /**< @brief Input: Info specific to fetch channel   */
    uint8_t             name[UPDATE_MAX_FILENAME];  /**< @brief Input: File Name                    */
} UPDATE_InstallCfgMsgType;

/**
    @brief Message to perform installation of new SW version

    @trace #BRCM_SWREQ_UPDATE
 */
typedef struct sUPDATE_InstallMsgType {
    UPDATE_InstallCfgMsgType cfg;            /**< @brief Input: Install configuration                    */
    uint32_t                 recvFileSize;   /**< @brief Output: Received file size                      */
} UPDATE_InstallMsgType;

int32_t UPDATE_HealthCheck(BCM_HandleType aConnHdl, PTBL_IdType aPid, IMGL_VersionType *aVersion);

uint32_t UPDATE_InstallHost(BCM_HandleType aConnHdl, UPDATE_InstallCfgMsgType *aInstallMsg,
    uint32_t *aRecvFileSize, BCM_MsgType aCmd);

int32_t UPDATE_FullInstall(BCM_HandleType aConnHdl, UPDATE_InstallCfgMsgType *aInstallMsg,
    uint32_t *aRecvFileSize);

#endif // BCM_UPDATE_H
