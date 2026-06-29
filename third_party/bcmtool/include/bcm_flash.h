#ifndef BCM_FLASH_H
#define BCM_FLASH_H

#include <stdint.h>
#include <stdlib.h>

#include "bcm_common.h"

/**
    @name NVM FLASH Abstract Interface IDs
    @{
    @brief Interface IDs for NVM FLASH Abstract

*/
#define BRCM_SWARCH_FLASH_ERASESECTOR_CMD_PROC         (0x8200U)    /**< @brief #FLASH_EraseSector       */
#define BRCM_SWARCH_FLASH_WRITEPAGE_CMD_PROC           (0x8201U)    /**< @brief #FLASH_WritePage         */
#define BRCM_SWARCH_FLASH_READPAGE_CMD_PROC            (0x8202U)    /**< @brief #FLASH_ReadPage          */

#define BRCM_SWARCH_FLASH_RPC_MAX_MACRO                (0x8210U)    /**< @brief #FLASH_RPC_MAX_DATA_SIZE */
#define BRCM_SWARCH_FLASH_CMD_TYPE                     (0x8211U)    /**< @brief #FLASH_CmdIDType         */
#define BRCM_SWARCH_FLASH_RPC_READ_INFO_TYPE           (0x8212U)    /**< @brief #FLASH_RpcReadInfoType   */
#define BRCM_SWARCH_FLASH_RPC_WRITE_INFO_TYPE          (0x8213U)    /**< @brief #FLASH_RpcWriteInfoType  */
#define BRCM_SWARCH_FLASH_RPC_ERASE_INFO_TYPE          (0x8214U)    /**< @brief #FLASH_RpcEraseInfoType  */
#define BRCM_SWARCH_FLASH_HANDLE_TYPE                  (0x8215U)    /**< @brief #FLASH_HandleType        */
#define BRCM_SWARCH_FLASH_MSG_TYPE                     (0x8216U)    /**< @brief #FLASH_MsgType           */
#define BRCM_SWARCH_FLASH_IMGL_CMD_INFO_TYPE           (0x8217U)    /**< @brief #FLASH_ImglCmdInfoType   */
#define BRCM_SWARCH_FLASH_MAX_PAGE_SIZE_MACRO          (0x8218U)    /**< @brief #FLASH_MAX_PAGE_SIZE     */
#define BRCM_SWARCH_FLASH_MAX_FILESIZE_MACRO           (0x8219U)    /**< @brief #FLASH_MAX_FILESIZE      */
#define BRCM_SWARCH_FLASH_MAX_SECTOR_MACRO             (0x821AU)    /**< @brief #FLASH_MAX_SECTOR        */
#define BRCM_SWARCH_FLASH_MAX_PT_COUNT_MACRO           (0x821BU)    /**< @brief #FLASH_MAX_PT_COUNT      */
#define BRCM_SWARCH_FLASH_ID_OF_MACRO                  (0x821CU)    /**< @brief #FLASH_ID_OF             */
/** @} */

/**
    @brief NVM Integer parse status

    @trace #BRCM_SWREQ_FLASH
*/
#define  FLASH_MAX_PAGE_SIZE    (256UL)

/**
    @brief NVM Integer parse status

    @trace #BRCM_SWREQ_FLASH
*/
#define FLASH_MAX_FILESIZE    (2UL*1024UL*1024UL)

/**
    @brief NVM Integer parse status

    @trace #BRCM_SWREQ_FLASH
*/
#define FLASH_MAX_SECTOR           (32UL)
#define FLASH_MAX_SECTOR_SIZE      (0x10000UL)

/**
    @brief NVM maximum count of PT images

    @trace #BRCM_SWREQ_FLASH
*/
#define FLASH_MAX_PT_COUNT         (16UL)


/**
    @brief Flash RPC operation max data size

    @trace #BRCM_SWREQ_FLASH
 */
#define FLASH_RPC_MAX_DATA_SIZE             (256UL)
#define FLASH_RPC_MAX_PAYLOAD_SIZE          (FLASH_RPC_MAX_DATA_SIZE + 32UL)

/**
    @brief Macro to Construct Flash CmdID

    @trace #BRCM_SWREQ_FLASH
*/
#define FLASH_ID_OF(aId)     \
    BCM_MSG(BCM_GROUPID_NVM, BCM_FLM_ID, aId)

/**
  @name Flash module supported command
  @{
  @brief Flash module supported command

  @trace #BRCM_SWREQ_FLASH
*/
typedef uint32_t FLASH_CmdIDType;            /**< @brief typedef for flash cmdID */
#define FLASH_CMD_RPC_READ                   FLASH_ID_OF(1UL) /**< @brief #FLASH_RpcReadInfoType */
#define FLASH_CMD_RPC_WRITE                  FLASH_ID_OF(2UL) /**< @brief #FLASH_RpcWriteInfoType */
#define FLASH_CMD_RPC_ERASE                  FLASH_ID_OF(3UL) /**< @brief #FLASH_RpcEraseInfoType */
#define FLASH_CMD_IMGL_READ                  FLASH_ID_OF(4UL) /**< @brief #FLASH_ImglCmdInfoType */
#define FLASH_CMD_IMGL_WRITE                 FLASH_ID_OF(5UL) /**< @brief #FLASH_ImglCmdInfoType */
#define FLASH_CMD_IMGL_ERASE                 FLASH_ID_OF(6UL) /**< @brief #FLASH_ImglCmdInfoType */
#define FLASH_CMD_IMGL_READ_V2               FLASH_ID_OF(7UL) /**< @brief #FLASH_ImglCmdInfoType */
#define FLASH_CMD_IMGL_COPY                  FLASH_ID_OF(8UL) /**< @brief #FLASH_ImglCmdInfoType */
/** @} */

/**
    @brief Flash Module RPC Read Info structure

    @trace #BRCM_SWREQ_FLASH
 */
typedef struct sFLASH_ImglCmdInfoType {
    uint32_t            hwID;                            /**< @brief  Flash ID */
    uint32_t            len;                             /**< @brief  Read/wrire/erase length */
    uint32_t            readAddr;                        /**< @brief  Flash Physical read address */
    uint32_t            writeAddr;                       /**< @brief  Flash Physical write/erase address */
    uint8_t             *buf;                            /**< @brief  Input/Output data buffer */
} FLASH_ImglCmdInfoType;

/**
    @brief Flash Module RPC Read Info structure

    @trace #BRCM_SWREQ_FLASH
 */
typedef struct sFLASH_RpcReadInfoType {
    uint32_t            hwID;                              /**< @brief  Flash ID */
    uint32_t            addr;                              /**< @brief  Flash Physical address */
    uint32_t            len;                               /**< @brief  Read length */
    uint8_t             bufOut[FLASH_RPC_MAX_DATA_SIZE];  /**< @brief  Input data buffer */
} FLASH_RpcReadInfoType;

/**
    @brief Flash Module RPC Write Info structure

    @trace #BRCM_SWREQ_FLASH
 */
typedef struct sFLASH_RpcWriteInfoType {
    uint32_t            hwID;                              /**< @brief Flash ID */
    uint32_t            addr;                              /**< @brief operation Physical address */
    uint32_t            len;                               /**< @brief write length */
    uint8_t             bufIn[FLASH_RPC_MAX_DATA_SIZE];   /**< @brief Output data buffer */
} FLASH_RpcWriteInfoType;

/**
    @brief Flash Module RPC Erase Info structure

    @trace #BRCM_SWREQ_FLASH
 */
typedef struct sFLASH_RpcEraseInfoType {
    uint32_t            hwID;                             /**< @brief  Flash ID */
    uint32_t            addr;                             /**< @brief  operation Physical address */
    uint32_t            len;                             /**< @brief  Flash sector / sub-sector size */
} FLASH_RpcEraseInfoType;

/**
  @brief Flash Module Command Handle

  @trace #BRCM_SWREQ_FLASH
*/
typedef union uFLASH_HandleType {
    FLASH_RpcReadInfoType  rpcReadInfo;
    FLASH_RpcWriteInfoType rpcWriteInfo;
    FLASH_RpcEraseInfoType rpcEraseInfo;
    FLASH_ImglCmdInfoType  imglCmdInfo;
    uint8_t                data[FLASH_RPC_MAX_PAYLOAD_SIZE];
} FLASH_HandleType;

/** @brief Erase flash

    API to erase flash sector.

    @behavior Async, Re-entrant

    @pre None

    @param[in]      aConnHdl    Connection handle (from RPC_Connect)
    @param[in]      aID         Controller ID
    @param[in]      aAddr       Flash start address for operation. It shall be
                                a Flash sector/sub-sector size aligned address

    Return values are documented in reverse-chronological order
    @retval     #BCM_ERR_OK             Successfully initiated flash erase
                                        operation
    @retval     #BCM_ERR_INVAL_PARAMS   (aID is invalid) or
                                        (aAddr is not aligned to sector boundary)
    @retval     #BCM_ERR_BUSY           API is called before completion of previous
                                        flash operation

    @post None

    @trace #BRCM_SWREQ_FLASH

    @limitations None
*/
extern int32_t FLASH_EraseSector(BCM_HandleType aConnHdl,
                                   uint32_t aID,
                                   uint32_t aAddr);

/** @brief Flash Page write

    API to write flash from aAddr  of a page size.

    @behavior Async, Re-entrant

    @pre None

    @param[in]      aConnHdl    Connection handle (from RPC_Connect)
    @param[in]      aID         Controller ID
    @param[in]      aAddr       Flash start address for operation. It shall be
                                Flash page size aligned address.
    @param[in]      aBuf        Pointer to input data buffer

    Return values are documented in reverse-chronological order
    @retval     #BCM_ERR_OK             Successfully initiated flash write operation
    @retval     #BCM_ERR_INVAL_PARAMS   (aID is invalid) or
                                        (aBuf is NULL) or
                                        (aAddr unaligned to flash page boundary) or
    @retval     #BCM_ERR_BUSY           API called before completion of previous
                                        flash operation

    @post None

    @trace #BRCM_SWREQ_FLASH

    @limitations None
*/
extern int32_t FLASH_WritePage(BCM_HandleType aConnHdl,
                                uint32_t aID,
                                uint32_t aAddr,
                                const uint8_t *const aBuf);

/** @brief Flash Page read

    API to read flash from aAddr of a page size.

    @behavior Async, Re-entrant

    @pre None

    @param[in]      aConnHdl    Connection handle (from RPC_Connect)
    @param[in]      aID         Controller ID
    @param[in]      aAddr       Flash address
    @param[out]     aBuf        Pointer to output data buffer

    Return values are documented in reverse-chronological order
    @retval     #BCM_ERR_OK             Successfully initiated flash read operation
    @retval     #BCM_ERR_INVAL_PARAMS   (aID is invalid) or
                                        (aBuf is NULL) or
                                        (aAddr unaligned to flash page boundary) or
    @retval     #BCM_ERR_BUSY           API called when previous operation in progress

    @post None

    @trace #BRCM_SWREQ_FLASH

    @limitations None
*/
extern int32_t FLASH_ReadPage(BCM_HandleType aConnHdl,
                                uint32_t aID,
                                uint32_t aAddr,
                                uint8_t *const aBuf);

#endif // BCM_FLASH_H