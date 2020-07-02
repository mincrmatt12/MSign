#ifndef MOCK_SYSTEM_H
#define MOCK_SYSTEM_H

#include <stdint.h>
#include <unistd.h>

extern char ** orig_argv;

[[noreturn]] inline void NVIC_SystemReset() {
	execve(orig_argv[0], orig_argv, nullptr);
	while (1) {;}
}

#define SET_BIT(REG, BIT)     ((REG) |= (BIT))

#define CLEAR_BIT(REG, BIT)   ((REG) &= ~(BIT))

#define READ_BIT(REG, BIT)    ((REG) & (BIT))

#define CLEAR_REG(REG)        ((REG) = (0x0))

#define WRITE_REG(REG, VAL)   ((REG) = (VAL))

#define READ_REG(REG)         ((REG))

struct {
	uint32_t KEYR, CR, SR, OPTKEYR, OPTCR, ACR;
} _FLASH;

auto* FLASH = &_FLASH;

#define FLASH_KEY1               0x45670123U
#define FLASH_KEY2               0xCDEF89ABU

#define FLASH_ACR_LATENCY_Pos          (0U)                                    
#define FLASH_ACR_LATENCY_Msk          (0xFU << FLASH_ACR_LATENCY_Pos)         /*!< 0x0000000F */
#define FLASH_ACR_LATENCY              FLASH_ACR_LATENCY_Msk                   
#define FLASH_ACR_LATENCY_0WS          0x00000000U                             
#define FLASH_ACR_LATENCY_1WS          0x00000001U                             
#define FLASH_ACR_LATENCY_2WS          0x00000002U                             
#define FLASH_ACR_LATENCY_3WS          0x00000003U                             
#define FLASH_ACR_LATENCY_4WS          0x00000004U                             
#define FLASH_ACR_LATENCY_5WS          0x00000005U                             
#define FLASH_ACR_LATENCY_6WS          0x00000006U                             
#define FLASH_ACR_LATENCY_7WS          0x00000007U                             

#define FLASH_ACR_PRFTEN_Pos           (8U)                                    
#define FLASH_ACR_PRFTEN_Msk           (0x1U << FLASH_ACR_PRFTEN_Pos)          /*!< 0x00000100 */
#define FLASH_ACR_PRFTEN               FLASH_ACR_PRFTEN_Msk                    
#define FLASH_ACR_ICEN_Pos             (9U)                                    
#define FLASH_ACR_ICEN_Msk             (0x1U << FLASH_ACR_ICEN_Pos)            /*!< 0x00000200 */
#define FLASH_ACR_ICEN                 FLASH_ACR_ICEN_Msk                      
#define FLASH_ACR_DCEN_Pos             (10U)                                   
#define FLASH_ACR_DCEN_Msk             (0x1U << FLASH_ACR_DCEN_Pos)            /*!< 0x00000400 */
#define FLASH_ACR_DCEN                 FLASH_ACR_DCEN_Msk                      
#define FLASH_ACR_ICRST_Pos            (11U)                                   
#define FLASH_ACR_ICRST_Msk            (0x1U << FLASH_ACR_ICRST_Pos)           /*!< 0x00000800 */
#define FLASH_ACR_ICRST                FLASH_ACR_ICRST_Msk                     
#define FLASH_ACR_DCRST_Pos            (12U)                                   
#define FLASH_ACR_DCRST_Msk            (0x1U << FLASH_ACR_DCRST_Pos)           /*!< 0x00001000 */
#define FLASH_ACR_DCRST                FLASH_ACR_DCRST_Msk                     
#define FLASH_ACR_BYTE0_ADDRESS_Pos    (10U)                                   
#define FLASH_ACR_BYTE0_ADDRESS_Msk    (0x10008FU << FLASH_ACR_BYTE0_ADDRESS_Pos) /*!< 0x40023C00 */
#define FLASH_ACR_BYTE0_ADDRESS        FLASH_ACR_BYTE0_ADDRESS_Msk             
#define FLASH_ACR_BYTE2_ADDRESS_Pos    (0U)                                    
#define FLASH_ACR_BYTE2_ADDRESS_Msk    (0x40023C03U << FLASH_ACR_BYTE2_ADDRESS_Pos) /*!< 0x40023C03 */
#define FLASH_ACR_BYTE2_ADDRESS        FLASH_ACR_BYTE2_ADDRESS_Msk             

/*******************  Bits definition for FLASH_SR register  ******************/
#define FLASH_SR_EOP_Pos               (0U)                                    
#define FLASH_SR_EOP_Msk               (0x1U << FLASH_SR_EOP_Pos)              /*!< 0x00000001 */
#define FLASH_SR_EOP                   FLASH_SR_EOP_Msk                        
#define FLASH_SR_SOP_Pos               (1U)                                    
#define FLASH_SR_SOP_Msk               (0x1U << FLASH_SR_SOP_Pos)              /*!< 0x00000002 */
#define FLASH_SR_SOP                   FLASH_SR_SOP_Msk                        
#define FLASH_SR_WRPERR_Pos            (4U)                                    
#define FLASH_SR_WRPERR_Msk            (0x1U << FLASH_SR_WRPERR_Pos)           /*!< 0x00000010 */
#define FLASH_SR_WRPERR                FLASH_SR_WRPERR_Msk                     
#define FLASH_SR_PGAERR_Pos            (5U)                                    
#define FLASH_SR_PGAERR_Msk            (0x1U << FLASH_SR_PGAERR_Pos)           /*!< 0x00000020 */
#define FLASH_SR_PGAERR                FLASH_SR_PGAERR_Msk                     
#define FLASH_SR_PGPERR_Pos            (6U)                                    
#define FLASH_SR_PGPERR_Msk            (0x1U << FLASH_SR_PGPERR_Pos)           /*!< 0x00000040 */
#define FLASH_SR_PGPERR                FLASH_SR_PGPERR_Msk                     
#define FLASH_SR_PGSERR_Pos            (7U)                                    
#define FLASH_SR_PGSERR_Msk            (0x1U << FLASH_SR_PGSERR_Pos)           /*!< 0x00000080 */
#define FLASH_SR_PGSERR                FLASH_SR_PGSERR_Msk                     
#define FLASH_SR_BSY_Pos               (16U)                                   
#define FLASH_SR_BSY_Msk               (0x1U << FLASH_SR_BSY_Pos)              /*!< 0x00010000 */
#define FLASH_SR_BSY                   FLASH_SR_BSY_Msk                        

/*******************  Bits definition for FLASH_CR register  ******************/
#define FLASH_CR_PG_Pos                (0U)                                    
#define FLASH_CR_PG_Msk                (0x1U << FLASH_CR_PG_Pos)               /*!< 0x00000001 */
#define FLASH_CR_PG                    FLASH_CR_PG_Msk                         
#define FLASH_CR_SER_Pos               (1U)                                    
#define FLASH_CR_SER_Msk               (0x1U << FLASH_CR_SER_Pos)              /*!< 0x00000002 */
#define FLASH_CR_SER                   FLASH_CR_SER_Msk                        
#define FLASH_CR_MER_Pos               (2U)                                    
#define FLASH_CR_MER_Msk               (0x1U << FLASH_CR_MER_Pos)              /*!< 0x00000004 */
#define FLASH_CR_MER                   FLASH_CR_MER_Msk                        
#define FLASH_CR_SNB_Pos               (3U)                                    
#define FLASH_CR_SNB_Msk               (0x1FU << FLASH_CR_SNB_Pos)             /*!< 0x000000F8 */
#define FLASH_CR_SNB                   FLASH_CR_SNB_Msk                        
#define FLASH_CR_SNB_0                 (0x01U << FLASH_CR_SNB_Pos)             /*!< 0x00000008 */
#define FLASH_CR_SNB_1                 (0x02U << FLASH_CR_SNB_Pos)             /*!< 0x00000010 */
#define FLASH_CR_SNB_2                 (0x04U << FLASH_CR_SNB_Pos)             /*!< 0x00000020 */
#define FLASH_CR_SNB_3                 (0x08U << FLASH_CR_SNB_Pos)             /*!< 0x00000040 */
#define FLASH_CR_SNB_4                 (0x10U << FLASH_CR_SNB_Pos)             /*!< 0x00000080 */
#define FLASH_CR_PSIZE_Pos             (8U)                                    
#define FLASH_CR_PSIZE_Msk             (0x3U << FLASH_CR_PSIZE_Pos)            /*!< 0x00000300 */
#define FLASH_CR_PSIZE                 FLASH_CR_PSIZE_Msk                      
#define FLASH_CR_PSIZE_0               (0x1U << FLASH_CR_PSIZE_Pos)            /*!< 0x00000100 */
#define FLASH_CR_PSIZE_1               (0x2U << FLASH_CR_PSIZE_Pos)            /*!< 0x00000200 */
#define FLASH_CR_STRT_Pos              (16U)                                   
#define FLASH_CR_STRT_Msk              (0x1U << FLASH_CR_STRT_Pos)             /*!< 0x00010000 */
#define FLASH_CR_STRT                  FLASH_CR_STRT_Msk                       
#define FLASH_CR_EOPIE_Pos             (24U)                                   
#define FLASH_CR_EOPIE_Msk             (0x1U << FLASH_CR_EOPIE_Pos)            /*!< 0x01000000 */
#define FLASH_CR_EOPIE                 FLASH_CR_EOPIE_Msk                      
#define FLASH_CR_LOCK_Pos              (31U)                                   
#define FLASH_CR_LOCK_Msk              (0x1U << FLASH_CR_LOCK_Pos)             /*!< 0x80000000 */
#define FLASH_CR_LOCK                  FLASH_CR_LOCK_Msk                       

/*******************  Bits definition for FLASH_OPTCR register  ***************/
#define FLASH_OPTCR_OPTLOCK_Pos        (0U)                                    
#define FLASH_OPTCR_OPTLOCK_Msk        (0x1U << FLASH_OPTCR_OPTLOCK_Pos)       /*!< 0x00000001 */
#define FLASH_OPTCR_OPTLOCK            FLASH_OPTCR_OPTLOCK_Msk                 
#define FLASH_OPTCR_OPTSTRT_Pos        (1U)                                    
#define FLASH_OPTCR_OPTSTRT_Msk        (0x1U << FLASH_OPTCR_OPTSTRT_Pos)       /*!< 0x00000002 */
#define FLASH_OPTCR_OPTSTRT            FLASH_OPTCR_OPTSTRT_Msk                 
#define FLASH_OPTCR_BOR_LEV_0          0x00000004U                             
#define FLASH_OPTCR_BOR_LEV_1          0x00000008U                             
#define FLASH_OPTCR_BOR_LEV_Pos        (2U)                                    
#define FLASH_OPTCR_BOR_LEV_Msk        (0x3U << FLASH_OPTCR_BOR_LEV_Pos)       /*!< 0x0000000C */
#define FLASH_OPTCR_BOR_LEV            FLASH_OPTCR_BOR_LEV_Msk                 

#define FLASH_OPTCR_WDG_SW_Pos         (5U)                                    
#define FLASH_OPTCR_WDG_SW_Msk         (0x1U << FLASH_OPTCR_WDG_SW_Pos)        /*!< 0x00000020 */
#define FLASH_OPTCR_WDG_SW             FLASH_OPTCR_WDG_SW_Msk                  
#define FLASH_OPTCR_nRST_STOP_Pos      (6U)                                    
#define FLASH_OPTCR_nRST_STOP_Msk      (0x1U << FLASH_OPTCR_nRST_STOP_Pos)     /*!< 0x00000040 */
#define FLASH_OPTCR_nRST_STOP          FLASH_OPTCR_nRST_STOP_Msk               
#define FLASH_OPTCR_nRST_STDBY_Pos     (7U)                                    
#define FLASH_OPTCR_nRST_STDBY_Msk     (0x1U << FLASH_OPTCR_nRST_STDBY_Pos)    /*!< 0x00000080 */
#define FLASH_OPTCR_nRST_STDBY         FLASH_OPTCR_nRST_STDBY_Msk              
#define FLASH_OPTCR_RDP_Pos            (8U)                                    
#define FLASH_OPTCR_RDP_Msk            (0xFFU << FLASH_OPTCR_RDP_Pos)          /*!< 0x0000FF00 */
#define FLASH_OPTCR_RDP                FLASH_OPTCR_RDP_Msk                     
#define FLASH_OPTCR_RDP_0              (0x01U << FLASH_OPTCR_RDP_Pos)          /*!< 0x00000100 */
#define FLASH_OPTCR_RDP_1              (0x02U << FLASH_OPTCR_RDP_Pos)          /*!< 0x00000200 */
#define FLASH_OPTCR_RDP_2              (0x04U << FLASH_OPTCR_RDP_Pos)          /*!< 0x00000400 */
#define FLASH_OPTCR_RDP_3              (0x08U << FLASH_OPTCR_RDP_Pos)          /*!< 0x00000800 */
#define FLASH_OPTCR_RDP_4              (0x10U << FLASH_OPTCR_RDP_Pos)          /*!< 0x00001000 */
#define FLASH_OPTCR_RDP_5              (0x20U << FLASH_OPTCR_RDP_Pos)          /*!< 0x00002000 */
#define FLASH_OPTCR_RDP_6              (0x40U << FLASH_OPTCR_RDP_Pos)          /*!< 0x00004000 */
#define FLASH_OPTCR_RDP_7              (0x80U << FLASH_OPTCR_RDP_Pos)          /*!< 0x00008000 */
#define FLASH_OPTCR_nWRP_Pos           (16U)                                   
#define FLASH_OPTCR_nWRP_Msk           (0xFFFU << FLASH_OPTCR_nWRP_Pos)        /*!< 0x0FFF0000 */
#define FLASH_OPTCR_nWRP               FLASH_OPTCR_nWRP_Msk                    
#define FLASH_OPTCR_nWRP_0             (0x001U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x00010000 */
#define FLASH_OPTCR_nWRP_1             (0x002U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x00020000 */
#define FLASH_OPTCR_nWRP_2             (0x004U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x00040000 */
#define FLASH_OPTCR_nWRP_3             (0x008U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x00080000 */
#define FLASH_OPTCR_nWRP_4             (0x010U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x00100000 */
#define FLASH_OPTCR_nWRP_5             (0x020U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x00200000 */
#define FLASH_OPTCR_nWRP_6             (0x040U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x00400000 */
#define FLASH_OPTCR_nWRP_7             (0x080U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x00800000 */
#define FLASH_OPTCR_nWRP_8             (0x100U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x01000000 */
#define FLASH_OPTCR_nWRP_9             (0x200U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x02000000 */
#define FLASH_OPTCR_nWRP_10            (0x400U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x04000000 */
#define FLASH_OPTCR_nWRP_11            (0x800U << FLASH_OPTCR_nWRP_Pos)        /*!< 0x08000000 */

inline void LL_FLASH_DisableDataCache() {}
inline void LL_FLASH_EnableDataCacheReset() {}
inline void LL_FLASH_DisableDataCacheReset() {}

#endif
