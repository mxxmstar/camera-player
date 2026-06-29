#ifndef BCM_DMON_H
#define BCM_DMON_H

#include <stdint.h>
#include <stdlib.h>

#include "bcm_common.h"

/**
    @name System Device Monitor Architecture IDs
    @{
    @brief Architecture IDs for System Device Monitor
*/
#define BRCM_SWARCH_DMON_READMEM_PROC                   (0x8200U)   /**< @brief #DMON_ReadMem           */
#define BRCM_SWARCH_DMON_WRITEMEM_PROC                  (0x8201U)   /**< @brief #DMON_WriteMem          */
#define BRCM_SWARCH_DMON_PING_PROC                      (0x8202U)   /**< @brief #DMON_Ping              */
#define BRCM_SWARCH_DMON_SYNC_PROC                      (0x8203U)   /**< @brief #DMON_Sync              */
#define BRCM_SWARCH_DMON_GETSWVERSION_PROC              (0x8204U)   /**< @brief #DMON_GetSwVersion      */
#define BRCM_SWARCH_DMON_GETHWVERSION_PROC              (0x8205U)   /**< @brief #DMON_GetHwVersion      */
#define BRCM_SWARCH_DMON_REBOOT_PROC                    (0x8206U)   /**< @brief #DMON_Reboot            */
#define BRCM_SWARCH_DMON_SYNC_WAIT_PROC                 (0x8207U)   /**< @brief #DMON_SyncWait          */
#define BRCM_SWARCH_DMON_DEEPSLEEP_PROC                 (0x8208U)   /**< @brief #DMON_DeepSleep           */

#define BRCM_SWARCH_DMON_MAX_MSG_SIZE_MACRO             (0x8210U)   /**< @brief #DMON_MAX_MSG_SIZE       */
#define BRCM_SWARCH_DMON_ID_MACRO                       (0x8211U)   /**< @brief #DMON_ID                 */
#define BRCM_SWARCH_DMON_ID_TYPE                        (0x8212U)   /**< @brief #DMON_IDType             */
#define BRCM_SWARCH_DMON_SECURITY_MODE_TYPE             (0x8213U)   /**< @brief #DMON_SecurityModeType   */
#define BRCM_SWARCH_DMON_DEVICE_TYPE                    (0x8214U)   /**< @brief #DMON_DeviceType         */

#define BRCM_SWARCH_DMON_PING_MSG_TYPE                  (0x8240U)   /**< @brief #DMON_PingMsgType        */
#define BRCM_SWARCH_DMON_MEM_ACCESS_MSG_TYPE            (0x8241U)   /**< @brief #DMON_MemAccessMsgType   */
#define BRCM_SWARCH_DMON_SW_VERSION_STRLEN_MACRO        (0x8242U)   /**< @brief #DMON_SW_VERSION_STR_LEN */
#define BRCM_SWARCH_DMON_SW_VERSION_MSG_TYPE            (0x8243U)   /**< @brief #DMON_SwVersionMsgType   */
#define BRCM_SWARCH_DMON_HW_VERSION_MSG_TYPE            (0x8244U)   /**< @brief #DMON_HwVersionMsgType   */
#define BRCM_SWARCH_DMON_REBOOT_MSG_TYPE                (0x8245U)   /**< @brief #DMON_RebootMsgType      */
#define BRCM_SWARCH_DMON_HEART_BEAT_MSG_TYPE            (0x8246U)   /**< @brief #DMON_HeartBeatMsgType   */
#define BRCM_SWARCH_DMON_TIME_TYPE                      (0x8247U)   /**< @brief #DMON_TimeType           */
#define BRCM_SWARCH_DMON_SYNC_MSG_TYPE                  (0x8248U)   /**< @brief #DMON_SyncMsgType        */
#define BRCM_SWARCH_DMON_DEEPSLEEP_MSG_TYPE             (0x8249U)   /**< @brief #DMON_DeepSleepMsgType   */

#define BRCM_SWARCH_DMON_MSG_UNION_TYPE                 (0x8280U)   /**< @brief #DMON_MsgUnionType      */
#define BRCM_SWARCH_DMON_MSG_TYPE                       (0x8281U)   /**< @brief #DMON_MsgType           */

/** @} */

/**
    @brief Device Monitor Max message size

    @trace #BRCM_SWREQ_DMON
*/
#define DMON_MAX_MSG_SIZE   (256UL)


/**
    @name DMN ID Macros
    @{
    @brief DMN ID construction macro

    @trace #BRCM_SWREQ_DMON
*/
#define DMON_ID(x)       BCM_MSG(BCM_GROUPID_SYS, BCM_DMN_ID, (x))
#define DMON_ID_ASYNC(x) BCM_MSG_ASYNC(BCM_GROUPID_SYS, BCM_DMN_ID, (x))
/** @} */

/**
    @name DMN Message IDs
    @{
    @brief Message IDs for exchange on message queues and Host

    These are used for interaction over MSGQ interface and RPC commands.
    Hence, this must be within 8-bit space.

    @trace #BRCM_SWREQ_DMON
*/
typedef BCM_MsgType DMON_IDType;     /**< @brief IMGL message ID Type */
#define DMON_ID_PING             DMON_ID(0x01U)   /**< @brief #DMON_PingMsgType        #None           */
#define DMON_ID_SYNC             DMON_ID(0x02U)   /**< @brief #DMON_SyncMsgType        #None           */
#define DMON_ID_SYNC_WAIT        DMON_ID(0x03U)   /**< @brief #DMON_SyncMsgType        #None           */

#define DMON_ID_MEM_WRITE        DMON_ID(0x10U)   /**< @brief #DMON_MemAccessMsgType   #DBGMEM_Write   */
#define DMON_ID_MEM_READ         DMON_ID(0x11U)   /**< @brief #DMON_MemAccessMsgType   #DBGMEM_Read    */

#define DMON_ID_SW_VERSION       DMON_ID(0x20U)   /**< @brief #DMON_SwVersionMsgType   #None           */
#define DMON_ID_HW_VERSION       DMON_ID(0x21U)   /**< @brief #DMON_HwVersionMsgType...#MCU_GetVersion */
#define DMON_ID_REBOOT           DMON_ID(0x22U)   /**< @brief #DMON_RebootMsgType      #MCU_RebootReq  */
#define DMON_ID_DEEPSLEEP        DMON_ID(0x23U)   /**< @brief #DMON_DeepSleepMsgType   #None           */
#define DMON_ID_LAST_HEART_BEAT  DMON_ID(0x24U)   /**< @brief #DMON_HeartBeatMsgType   #None           */

#define DMON_ID_HEART_BEAT       DMON_ID_ASYNC(0x01U) /**< @brief #DMON_HeartBeatMsgType   #None       */
/** @} */

/**
   @name Device Master Slave ID
   @{
   @trace #BRCM_SWREQ_DMON
 */
typedef uint32_t DMON_DeviceType;
#define DMON_DEVICE_MASTER          (0UL)  /**< @brief Device master mode */
#define DMON_DEVICE_SLAVE_1         (1UL)  /**< @brief Device ID slave_1 mode */
#define DMON_DEVICE_SLAVE_2         (2UL)  /**< @brief Device ID slave_2 mode */
/** @} */

/**
    @brief Memory Access Message

    @trace #BRCM_SWREQ_DMON
*/
typedef struct sDMON_MemAccessMsgType {
    uint32_t        addr;
    uint32_t        width;
    uint32_t        len;
    DMON_DeviceType deviceID;
    uint8_t         data[128UL];
} DMON_MemAccessMsgType;

/**
    @brief Software Version max length

    @trace #BRCM_SWREQ_DMON
*/
#define DMON_SW_VERSION_STR_LEN          (100UL)

/**
    @brief Software Version

    @trace #BRCM_SWREQ_DMON
*/
typedef struct sDMON_SwVersionMsgType {
    char      str[DMON_SW_VERSION_STR_LEN];
} DMON_SwVersionMsgType;

/**
    @name Device Security Mode Type
    @{
    @brief Device security modes

    @trace #BRCM_SWREQ_DMON
*/
typedef uint32_t DMON_SecurityModeType;     /**< @brief typedef for Device Security mode */
#define DMON_SECURITY_MODE_UNKNOWN (0x0UL)  /**< @brief Unknown mode */
#define DMON_SECURITY_MODE_NONE    (0x1UL)  /**< @brief Unsecured */
#define DMON_SECURITY_MODE_ECC     (0x2UL)  /**< @brief Secured through ECC */
#define DMON_SECURITY_MODE_RSA     (0x3UL)  /**< @brief Secured through RSA */
#define DMON_SECURITY_MODE_MAX     (DMON_SECURITY_MODE_RSA) /**< @brief Maximum mode value supported */
/** @} */

/**
    @brief Device Monitor HW version information structure

    @trace #BRCM_SWREQ_DMON
 */
typedef struct sDMON_HwVersionMsgType {
    uint32_t                manuf;  /**< @brief manufacturer ID */
    uint32_t                model;  /**< @brief model number */
    uint32_t                rev;    /**< @brief revision number */
    DMON_SecurityModeType   secMode;/**< @brief Security Mode */
} DMON_HwVersionMsgType;

/**
    @brief Ping Message (Response only, Command goes with empty payload)

    @trace #BRCM_SWREQ_DMON
*/
typedef struct sDMON_PingMsgType {
    BCM_BootModeType        mode;           /**< @brief mode: ROM/BL/FW             */
    DMON_HwVersionMsgType   version;        /**< @brief HW version                  */
} DMON_PingMsgType;

/**
    @brief Sync Message (Response only, Command goes with empty payload)

    @trace #BRCM_SWREQ_DMON
*/
typedef struct sDMON_SyncMsgType {
    BCM_BootModeType        mode;           /**< @brief mode: ROM/BL/FW             */
    BCM_StateType           state;          /**< @brief state: UNINIT/INIT/READY/RUN*/
    DMON_HwVersionMsgType   version;        /**< @brief HW version                  */
    uint64_t                initTime;       /**< @brief Init Time                   */
    uint64_t                readyTime;      /**< @brief Ready Time                  */
    uint64_t                runTime;        /**< @brief Run  Time                   */
    uint8_t                 rsvd[208];
} DMON_SyncMsgType;

/**
    @brief Reboot Message

    @trace #BRCM_SWREQ_DMON
*/
typedef struct sDMON_RebootMsgType {
    uint32_t            reserved[2UL];  /**< @brief left for legacy      */
    uint32_t            delayMs;        /**< @brief delay in milliseconds before rebooting */
} DMON_RebootMsgType;

/**
    @brief DeepSleep Message

    @trace #BRCM_SWREQ_DMON
*/
typedef struct sDMON_DeepSleepMsgType {
    uint32_t            delayMs;        /**< @brief delay in milliseconds before entering deepSleep mode */
} DMON_DeepSleepMsgType;

/**
    @brief Time info

    @trace #BRCM_SWREQ_DMON
*/
typedef struct sDMON_TimeType {
    uint32_t    s;
    uint32_t    ns;
} DMON_TimeType;

/**
    @brief Heartbeat Message

    @trace #BRCM_SWREQ_DMON
*/
typedef struct sDMON_HeartBeatMsgType {
    uint32_t        version;
    uint32_t        state;
    DMON_TimeType   upTime;
    uint32_t        count;
    uint32_t        voltage;
    uint32_t        voltage_1v0;
    uint32_t        voltage_1v8;
    uint32_t        voltage_3v3;
    uint32_t        temperature;
    uint32_t        rsvd[54UL];
} DMON_HeartBeatMsgType;

/**
    @brief Device Monitor Union encapsulating all messages

    @trace #BRCM_SWREQ_DMON
*/
typedef union uDMON_MsgUnionType {
    uint32_t                data[DMON_MAX_MSG_SIZE/4UL];
    DMON_PingMsgType        ping;
    DMON_SyncMsgType        sync;
    DMON_MemAccessMsgType   memAccess;
    DMON_SwVersionMsgType   swVersion;
    DMON_HwVersionMsgType   hwVersion;
    DMON_RebootMsgType      reboot;
    DMON_DeepSleepMsgType   deepSleep;
    DMON_HeartBeatMsgType   heartbeat;
} DMON_MsgUnionType;

/**
    @brief Device Monitor Structure for Message queue

    @trace #BRCM_SWREQ_DMON
*/
typedef struct sDMON_MsgType {
    uint32_t                    magic;
    BCM_MsgType                 id;
    int32_t                     status;
    uint32_t                    len;
    DMON_MsgUnionType           u;
} DMON_MsgType;

/**
    @brief Read Data from  Memory

    @behavior Sync, Non-Rentrant

    @pre None

    @param[in]      aHdl            Connection handle (from RPC_Connect)
    @param[in]      aAddr           Address to be Read
    @param[in]      aWidth          Data Width
    @param[in]      aDeviceID       Device ID
    @param[out]     aData           Data read from Memory

    Return values are documented in reverse-chronological order
    @retval         #BCM_ERR_OK               Success
    @retval         #BCM_ERR_INVAL_PARAMS     aData is NULL or
                                              aWidth is Invalid

    @post None

    @trace #BRCM_SWREQ_DMON

    @limitations None
*/
extern int32_t DMON_ReadMem(BCM_HandleType aHdl,
                             uint32_t        aAddr,
                             uint32_t        aWidth,
                             DMON_DeviceType aDeviceID,
                             uint32_t        *aData);

/**
    @brief Write Data to  Memory

    @behavior Sync, Non-Rentrant

    @pre None

    @param[in]      aHdl            Connection handle (from RPC_Connect)
    @param[in]      aAddr           Address to be Read
    @param[in]      aWidth          Data Width
    @param[in]      aData           Data to write in Memory
    @param[in]      aDeviceID       Device ID

    Return values are documented in reverse-chronological order
    @retval         #BCM_ERR_OK               Success
    @retval         #BCM_ERR_INVAL_PARAMS     aWidth is Invalid

    @post None

    @trace #BRCM_SWREQ_DMON

    @limitations None
*/
extern int32_t DMON_WriteMem(BCM_HandleType aHdl,
                             uint32_t        aAddr,
                             uint32_t        aWidth,
                             DMON_DeviceType aDeviceID,
                             uint32_t        aData);

/**
    @brief Ping Message

    @behavior Sync, Non-Rentrant

    @pre None

    @param[in]      aHdl            Connection handle (from RPC_Connect)
    @param[out]     aPing           Ping Message

    Return values are documented in reverse-chronological order
    @retval         #BCM_ERR_OK               Success
    @retval         #BCM_ERR_INVAL_PARAMS     aPing is NULL

    @post None

    @trace #BRCM_SWREQ_DMON

    @limitations None
*/
extern int32_t DMON_Ping(BCM_HandleType aHdl,
                          DMON_PingMsgType *aPing);

/**
    @brief Sync Message

    @behavior Sync, Non-Rentrant

    @pre None

    @param[in]      aHdl            Connection handle (from RPC_Connect)
    @param[out]     aSync           Sync Message

    Return values are documented in reverse-chronological order
    @retval         #BCM_ERR_OK               Success
    @retval         #BCM_ERR_INVAL_PARAMS     aSync is NULL

    @post None

    @trace #BRCM_SWREQ_DMON

    @limitations None
*/
extern int32_t DMON_Sync(BCM_HandleType aHdl, DMON_SyncMsgType *aSync);

/**
    @brief Wait until device reaches aState

    @behavior Sync, Non-Rentrant

    @pre None

    @param[in]      aHdl            Connection handle (from RPC_Connect)
    @param[in]      aState          State of the device to wait for
    @param[out]     aSync           Sync Message

    Return values are documented in reverse-chronological order
    @retval         #BCM_ERR_OK               Success
    @retval         #BCM_ERR_INVAL_PARAMS     aSync is NULL

    @post None

    @trace #BRCM_SWREQ_DMON

    @limitations None
*/
extern int32_t DMON_SyncWait(BCM_HandleType aHdl, BCM_StateType aState,
                            DMON_SyncMsgType *aSync);

/**
    @brief Get Software Version

    @behavior Sync, Non-Rentrant

    @pre None

    @param[in]      aHdl            Connection handle (from RPC_Connect)
    @param[out]     aSwVersion      Software Version

    Return values are documented in reverse-chronological order
    @retval         #BCM_ERR_OK               Success
    @retval         #BCM_ERR_INVAL_PARAMS     aSwVersion is NULL

    @post None

    @trace #BRCM_SWREQ_DMON

    @limitations None
*/
extern int32_t DMON_GetSwVersion(BCM_HandleType aHdl,
                                  DMON_SwVersionMsgType *aSwVersion);

/**
    @brief Get Hardware version

    @behavior Sync, Non-Rentrant

    @pre None

    @param[in]      aHdl            Connection handle (from RPC_Connect)
    @param[out]     aHwVersion      Hardware Version

    Return values are documented in reverse-chronological order
    @retval         #BCM_ERR_OK               Success
    @retval         #BCM_ERR_INVAL_PARAMS     aHwVersion is NULL

    @post None

    @trace #BRCM_SWREQ_DMON

    @limitations None
*/
extern int32_t DMON_GetHwVersion(BCM_HandleType aHdl,
                                  DMON_HwVersionMsgType *aHwVersion);

/**
    @brief Reboot the Device

    @behavior Sync, Non-Rentrant

    @pre None

    @param[in]      aHdl            Connection handle (from RPC_Connect)

    Return values are documented in reverse-chronological order
    @retval         #BCM_ERR_OK               Success

    @post None

    @trace #BRCM_SWREQ_DMON

    @limitations None
*/
extern int32_t DMON_Reboot(BCM_HandleType aHdl);

/**
    @brief Put the device in DeepSleep Mode

    This API initiates a transtition of all tc10 enabled transceivers to tc10
    sleep state and the remaining(except Port 8) to standby mode. Once the
    transceivers transition to expected state, the target responds back to host
    that the command has been accepted and then signals an external controller
    about its readiness to enter deep-sleep state. The external controller is
    expected to shutdown the power to the chip thereby putting it in Deep-Sleep

    A tc10 wake-up through on any of the tc10 enabled ports would signal the
    external chip to bring the chip out of deep-sleep.

    @behavior Sync, Non-Rentrant

    @pre None

    @param[in]      aHdl            Connection handle (from RPC_Connect)

    Return values are documented in reverse-chronological order
    @retval     #BCM_ERR_OK             Command accepted
    @retval     #BCM_ERR_INVAL_PARAMS   aHdl is invalid
    @retval     #BCM_ERR_NOSUPPORT      Unsupported
    @retval     #BCM_ERR_UNKNOWN        Unknown error

    @post None

    @trace #BRCM_SWREQ_DMON

    @limitations This API not supported on BCM8910X and BCM8908X platforms
*/
extern int32_t DMON_DeepSleep(BCM_HandleType aHdl);

#endif // BCM_DMON_H
