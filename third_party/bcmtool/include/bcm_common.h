#ifndef BCM_COMMON_H
#define BCM_COMMON_H

#include <stdint.h>
#include <string.h>

typedef uint64_t BCM_HandleType;

/**
    @name Boot Mode
    @{
    @brief Boot Mode

    @trace #BRCM_SWREQ_BCM_COMPONENT
*/
typedef uint32_t BCM_BootModeType;
#define BCM_BOOT_MODE_BROM      (0x42524F4DUL) /* 'B' 'R' 'O' 'M' */
#define BCM_BOOT_MODE_BL        (0x424C4452UL) /* 'B' 'L' 'D' 'R' */
#define BCM_BOOT_MODE_FW        (0x464D5752UL) /* 'F' 'M' 'W' 'R' */
#define BCM_BOOT_MODE_DEFAULT   (0x44464C54UL) /* 'D' 'F' 'L' 'T' */
/** @} */

/**
    @name Module State
    @{
    @brief Module State

    @trace #BRCM_SWREQ_BCM_COMPONENT
*/
typedef uint32_t BCM_StateType;    /**< @brief Subsystem states        */
#define BCM_STATE_UNINIT  (0UL)   /**< @brief Uninitialized state     */
#define BCM_STATE_INIT    (1UL)   /**< @brief Initialized state       */
#define BCM_STATE_READY   (2UL)   /**< @brief Ready state             */
#define BCM_STATE_RUN     (3UL)   /**< @brief Configured state        */
#define BCM_STATE_ERROR   (4UL)   /**< @brief Error state             */
/** @} */

/**
    @name Component IDs
    @{
    @brief 16-bit component IDs for all the components in the system

    @trace #BRCM_SWREQ_BCM_COMPONENT
*/
typedef uint16_t BCM_CompIDType;   /**< @brief 16-bit Unique Component ID for
                                    error reporting       */
/* BCM_GROUPID_RPC or Architecture */
#define BCM_RSV_ID (0x0000U)   /**< @brief Reserved       */
#define BCM_IPC_ID (0x0000U)   /**< @brief IPC            */
#define BCM_MOD_ID (0x0001U)   /**< @brief Module         */
#define BCM_MSG_ID (0x0002U)   /**< @brief MsgQ (IPC)     */
#define BCM_UTL_ID (0x0003U)   /**< @brief UTILS          */
#define BCM_OSI_ID (0x0004U)   /**< @brief OSI            */
#define BCM_DCA_ID (0x0005U)   /**< @brief DCache         */
#define BCM_PTU_ID (0x0006U)   /**< @brief PTU            */
#define BCM_RPC_ID (0x0022U)   /**< @brief RPC Module     */
#define BCM_RPS_ID (0x0040U)   /**< @brief RPC Service    */
#define BCM_INT_ID (0x00FFU)   /**< @brief Interrupt patch */

/* BCM_GROUPID_SYS */
#define BCM_MCU_ID (0x0100U)   /**< @brief MCU            */
#define BCM_WDG_ID (0x0101U)   /**< @brief Watchdog       */
#define BCM_DDR_ID (0x0102U)   /**< @brief DDR            */
#define BCM_VTM_ID (0x0103U)   /**< @brief VTMON          */
#define BCM_SPT_ID (0x0104U)   /**< @brief SP804          */
#define BCM_AVT_ID (0x0105U)   /**< @brief AVT            */
#define BCM_TMD_ID (0x0120U)   /**< @brief Time module    */
#define BCM_IMG_ID (0x0121U)   /**< @brief IMGL Module    */
/* Leave 0x0122 for backward compatibility                */
#define BCM_DMN_ID (0x0123U)   /**< @brief Device monitor */
#define BCM_PPR_ID (0x0130U)   /**< @brief PixelProcessor */
#define BCM_SYS_ID (0x0140U)   /**< @brief SYS            */
#define BCM_CTL_ID (0x0141U)   /**< @brief BL Control     */
#define BCM_ROM_ID (0x0142U)   /**< @brief ROM            */
/* BCM_GROUPID_NVM */
#define BCM_QSP_ID (0x0300U)   /**< @brief QSPI           */
#define BCM_OTP_ID (0x0301U)   /**< @brief OTP            */
#define BCM_PCH_ID (0x0302U)   /**< @brief PATCH          */
#define BCM_OTM_ID (0x0320U)   /**< @brief OTPM           */
#define BCM_FLM_ID (0x0321U)   /**< @brief Flash Manager  */
#define BCM_PTM_ID (0x0322U)   /**< @brief PTM            */
#define BCM_UPD_ID (0x0323U)   /**< @brief Update module  */
#define BCM_CFG_ID (0x0324U)   /**< @brief Config module  */
#define BCM_NVM_ID (0x0340U)   /**< @brief NVM            */

/**
    @name BCM Error Codes
    @{
    @brief Error return values

    @trace #BRCM_SWREQ_BCM_COMPONENT
*/
typedef int32_t BCM_ErrorType;
#define BCM_ERR_OK              (0x0)  /**< @brief No Error                          */
#define BCM_ERR_BUSY            (0x1)  /**< @brief Device or resource busy           */
#define BCM_ERR_NODEV           (0x2)  /**< @brief No device found                   */
#define BCM_ERR_NOT_FOUND       (0x3)  /**< @brief Not Found                         */
#define BCM_ERR_NOMEM           (0x4)  /**< @brief Out of memory                     */
#define BCM_ERR_NOSUPPORT       (0x5)  /**< @brief Not supported                     */
#define BCM_ERR_INVAL_PARAMS    (0x6)  /**< @brief Invalid argument                  */
#define BCM_ERR_INVAL_MAGIC     (0x7)  /**< @brief Invalid magic number              */
#define BCM_ERR_INVAL_STATE     (0x8)  /**< @brief Invalid state                     */
#define BCM_ERR_INVAL_BUF_STATE (0x9)  /**< @brief Invalid buffer state              */
#define BCM_ERR_EAGAIN          (0xA)  /**< @brief Try again                         */
#define BCM_ERR_TIME_OUT        (0xB)  /**< @brief Timeout                           */
#define BCM_ERR_UNINIT          (0xC)  /**< @brief Device or resource not initialized*/
#define BCM_ERR_CANCELLED       (0xD)  /**< @brief Cancel request success            */
#define BCM_ERR_DATA_INTEG      (0xE)  /**< @brief Data integrity error              */
#define BCM_ERR_AUTH_FAILED     (0xF)  /**< @brief Authentication error              */
#define BCM_ERR_VERSION         (0x10) /**< @brief Wrong version of hw/sw            */
#define BCM_ERR_BUS_FAILURE     (0x11) /**< @brief Bus Failure                       */
#define BCM_ERR_NACK            (0x12) /**< @brief NACK error                        */
#define BCM_ERR_MAX_ATTEMPS     (0x13) /**< @brief Maximum num of attempts           */
#define BCM_ERR_UNKNOWN         (0x14) /**< @brief Unknown error                     */
#define BCM_ERR_CUSTOM          (0x15) /**< @brief Module specific error             */
#define BCM_ERR_NOPERM          (0x16) /**< @brief Permission denied                 */
/** @} */

/**
    @brief System Command ID macros

    @trace #BRCM_SWREQ_BCM_COMPONENT
*/
typedef uint32_t BCM_MsgType;
#define BCM_MSG_ID_SHIFT                (0UL)                           /**< @brief Command ID shift value  */
#define BCM_MSG_ID_MASK                 (0xFFUL << BCM_MSG_ID_SHIFT)    /**< @brief Command ID shift mask   */
#define BCM_MSG_COMP_SHIFT              (8UL)                           /**< @brief Component ID shift value*/
#define BCM_MSG_COMP_MASK               (0xFFFFUL << BCM_MSG_COMP_SHIFT)/**< @brief Component ID shift mask */
#define BCM_MSG_GROUP_SHIFT             (24UL)                          /**< @brief Group ID shift value    */
#define BCM_MSG_GROUP_MASK              (0x3FUL << BCM_MSG_GROUP_SHIFT) /**< @brief Group ID shift mask     */
#define BCM_MSG_RESPONSE_SHIFT          (30UL)                          /**< @brief Resp bit Shift value    */
#define BCM_MSG_RESPONSE_MASK           (1UL << BCM_MSG_RESPONSE_SHIFT) /**< @brief Async bit Shift mask    */
#define BCM_MSG_ASYNC_SHIFT             (31UL)                          /**< @brief Async bit Shift value   */
#define BCM_MSG_ASYNC_MASK              (1UL << BCM_MSG_ASYNC_SHIFT)    /**< @brief Async bit Shift mask    */
#define BCM_MSG_MAGIC_CMD               (0xa5a5a5a5UL)                  /**< @brief Command magic           */
#define BCM_MSG_MAGIC_RESP              (0x5a5a5a5aUL)                  /**< @brief Response magic          */

/**
    @name RPC Memory Max counts
    @{
    @brief RPC Memory Max Counts

    @trace #BRCM_SWREQ_RPC
*/
#define RPC_MAX_CNT_MSG     (32UL)        /**< @brief Max Messages         */
#define RPC_MAX_CNT_APPINFO (4UL)         /**< @brief Max Appinfo per hdl  */
#define RPC_MSG_PAYLOAD_SZ  (448UL)
/** @} */

/**
    @name Group IDs
    @{
    @brief 6-bit group IDs

    @limitation Do not change the group ID of the IMGL group. This is being
    shared with BOOTROM

    @trace #BRCM_SWREQ_BCM_COMPONENT
*/
typedef uint8_t BCM_GroupIDType;
#define BCM_GROUPID_RPC         (0x00U) /**< @brief RPC group           */
#define BCM_GROUPID_SYS         (0x01U) /**< @brief System group        */
#define BCM_GROUPID_IO          (0x02U) /**< @brief System group        */
#define BCM_GROUPID_NVM         (0x03U) /**< @brief NVM group           */
#define BCM_GROUPID_CRYPTO      (0x04U) /**< @brief Crypto Group        */
#define BCM_GROUPID_ETHSRV      (0x05U) /**< @brief Communications group*/
#define BCM_GROUPID_XCVR        (0x06U) /**< @brief Communications group*/
#define BCM_GROUPID_DEBUG       (0x08U) /**< @brief Debug Group         */
#define BCM_GROUPID_APP         (0x09U) /**< @brief Application Group   */

#define BCM_GROUPID_AVCE        (0x20U) /**< @brief AVCE Group          */
#define BCM_GROUPID_AVCD        (0x21U) /**< @brief AVCD Group          */
#define BCM_GROUPID_CAMERA      (0x22U) /**< @brief Camera Group        */
#define BCM_GROUPID_OPENVX      (0x23U) /**< @brief OPENVX Group        */
#define BCM_GROUPID_LDC         (0x24U) /**< @brief LDC Group           */
#define BCM_GROUPID_DISPLAY     (0x25U) /**< @brief Display Group       */
#define BCM_GROUPID_GRAPHICS    (0x26U) /**< @brief Graphics Group      */
#define BCM_GROUPID_STITCH      (0x27U) /**< @brief Stitch Group        */
#define BCM_GROUPID_AUDIO       (0x28U) /**< @brief Audio Group         */
#define BCM_GROUPID_MJPEG       (0x29U) /**< @brief MJPEG Group         */
#define BCM_GROUPID_VPPE        (0x30U) /**< @brief Video pre processor */
#define BCM_GROUPID_TEST        (0x3EU) /**< @brief Test Group          */
#define BCM_GROUPID_INVALID     (0xFFU) /**< @brief Invalid group       */
/** @} */

/**
    @brief Macro to compose the entire command ID

    @trace #BRCM_SWREQ_BCM_COMPONENT
*/
#define BCM_MSG(aGrp, aComp, aId) ((((uint32_t)(aGrp) << BCM_MSG_GROUP_SHIFT) & BCM_MSG_GROUP_MASK)  \
                                        |(((uint32_t)(aComp) << BCM_MSG_COMP_SHIFT) & BCM_MSG_COMP_MASK)   \
                                        |(((uint32_t)(aId) << BCM_MSG_ID_SHIFT) & BCM_MSG_ID_MASK))


static inline void BCM_MemSet(void *aStr, uint8_t aVal, uint32_t aSize)
{
    ((void)memset(aStr, (int32_t)aVal, aSize));
}

/** @brief Copies aSize number bytes from aSrc to aDest.

    @behavior Sync, Re-entrant

    @pre None

    @param[inout]    aDest   Pointer to the destination array where the content is to be copied.
    @param[in]       aSrc    Pointer to the source of data to be copied
    @param[in]       aSize   Number of bytes to copy

    @retval     void

    @post None

    @trace  #BRCM_SWREQ_BCM_STDLIB_WRAPPER
*/
static inline void BCM_MemCpy(void *aDest, const void *aSrc, uint32_t aSize)
{
    ((void)memcpy(aDest, aSrc, aSize));
}

/** @brief converts 32-bit data from little endian format to Native's endian format.

    @limitations None
    @trace #BRCM_SWARCH_CPU_LETONATIVE32_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    return aData
    @endcode
*/
uint32_t CPU_LEToNative32(uint32_t aData);

/** @brief converts 32-bit data from Native's endian format to little endian format.

    @limitations None
    @trace #BRCM_SWARCH_CPU_NATIVETOLE32_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    return aData
    @endcode
*/
uint32_t CPU_NativeToLE32(uint32_t aData);

/** @brief converts 16-bit data from Native's endian format to little endian format.

    @limitations None
    @trace #BRCM_SWARCH_CPU_NATIVETOLE16_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    return aData
    @endcode
*/
uint16_t CPU_NativeToLE16(uint16_t aData);

/** @brief converts 16-bit data from big endian format to Native's endian format.

    @limitations None
    @trace #BRCM_SWARCH_CPU_BETONATIVE16_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    return CPU_NativeToBE16(aData)
    @endcode
*/
uint16_t CPU_BEToNative16(uint16_t aData);

/** @brief converts 32-bit data from big endian format to Native's endian format.

    @limitations None
    @trace #BRCM_SWARCH_CPU_BETONATIVE32_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    return CPU_NativeToBE32(aData)
    @endcode
*/
uint32_t CPU_BEToNative32(uint32_t aData);

/** @brief converts 64-bit data from little endian To Native's endian format

    @trace #BRCM_SWARCH_CPU_LETONATIVE64_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    @endcode
*/
uint64_t CPU_LEToNative64(uint64_t aData);

uint32_t ByteToU32(const uint8_t *bytes);

#endif // BCM_COMMON_H
