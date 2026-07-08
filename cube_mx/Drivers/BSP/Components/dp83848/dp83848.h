#ifndef DP83848_H
#define DP83848_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DP83848_BMCR      ((uint16_t)0x0000U)
#define DP83848_BMSR      ((uint16_t)0x0001U)
#define DP83848_PHYIDR1   ((uint16_t)0x0002U)
#define DP83848_PHYIDR2   ((uint16_t)0x0003U)
#define DP83848_ANAR      ((uint16_t)0x0004U)
#define DP83848_ANLPAR    ((uint16_t)0x0005U)
#define DP83848_ANER      ((uint16_t)0x0006U)
#define DP83848_PHYSTS    ((uint16_t)0x0010U)
#define DP83848_MICR      ((uint16_t)0x0011U)
#define DP83848_MISR      ((uint16_t)0x0012U)
#define DP83848_FCSCR     ((uint16_t)0x0014U)
#define DP83848_RECR      ((uint16_t)0x0015U)
#define DP83848_RBR       ((uint16_t)0x0017U)
#define DP83848_PHYCR     ((uint16_t)0x0019U)

#define DP83848_BMCR_SOFT_RESET       ((uint16_t)0x8000U)
#define DP83848_BMCR_LOOPBACK         ((uint16_t)0x4000U)
#define DP83848_BMCR_SPEED_SELECT     ((uint16_t)0x2000U)
#define DP83848_BMCR_AUTONEGO_EN      ((uint16_t)0x1000U)
#define DP83848_BMCR_POWER_DOWN       ((uint16_t)0x0800U)
#define DP83848_BMCR_ISOLATE          ((uint16_t)0x0400U)
#define DP83848_BMCR_RESTART_AUTONEGO ((uint16_t)0x0200U)
#define DP83848_BMCR_DUPLEX_MODE      ((uint16_t)0x0100U)

#define DP83848_BMSR_AUTONEGO_CPLT    ((uint16_t)0x0020U)
#define DP83848_BMSR_LINK_STATUS      ((uint16_t)0x0004U)

#define DP83848_PHYSTS_AUTONEGO_DONE  ((uint16_t)0x0010U)
#define DP83848_PHYSTS_DUPLEX_STATUS  ((uint16_t)0x0004U)
#define DP83848_PHYSTS_SPEED_10M      ((uint16_t)0x0002U)
#define DP83848_PHYSTS_LINK_STATUS    ((uint16_t)0x0001U)

#define DP83848_PHYCR_PHY_ADDR        ((uint16_t)0x001FU)

#define DP83848_STATUS_READ_ERROR            ((int32_t)-5)
#define DP83848_STATUS_WRITE_ERROR           ((int32_t)-4)
#define DP83848_STATUS_ADDRESS_ERROR         ((int32_t)-3)
#define DP83848_STATUS_RESET_TIMEOUT         ((int32_t)-2)
#define DP83848_STATUS_ERROR                 ((int32_t)-1)
#define DP83848_STATUS_OK                    ((int32_t) 0)
#define DP83848_STATUS_LINK_DOWN             ((int32_t) 1)
#define DP83848_STATUS_100MBITS_FULLDUPLEX   ((int32_t) 2)
#define DP83848_STATUS_100MBITS_HALFDUPLEX   ((int32_t) 3)
#define DP83848_STATUS_10MBITS_FULLDUPLEX    ((int32_t) 4)
#define DP83848_STATUS_10MBITS_HALFDUPLEX    ((int32_t) 5)
#define DP83848_STATUS_AUTONEGO_NOTDONE      ((int32_t) 6)

typedef int32_t (*dp83848_Init_Func)(void);
typedef int32_t (*dp83848_DeInit_Func)(void);
typedef int32_t (*dp83848_ReadReg_Func)(uint32_t, uint32_t, uint32_t *);
typedef int32_t (*dp83848_WriteReg_Func)(uint32_t, uint32_t, uint32_t);
typedef int32_t (*dp83848_GetTick_Func)(void);

typedef struct {
  dp83848_Init_Func Init;
  dp83848_DeInit_Func DeInit;
  dp83848_WriteReg_Func WriteReg;
  dp83848_ReadReg_Func ReadReg;
  dp83848_GetTick_Func GetTick;
} dp83848_IOCtx_t;

typedef struct {
  uint32_t DevAddr;
  uint32_t Is_Initialized;
  dp83848_IOCtx_t IO;
  void *pData;
} dp83848_Object_t;

int32_t DP83848_RegisterBusIO(dp83848_Object_t *pObj, dp83848_IOCtx_t *ioctx);
int32_t DP83848_Init(dp83848_Object_t *pObj);
int32_t DP83848_DeInit(dp83848_Object_t *pObj);
int32_t DP83848_DisablePowerDownMode(dp83848_Object_t *pObj);
int32_t DP83848_EnablePowerDownMode(dp83848_Object_t *pObj);
int32_t DP83848_StartAutoNego(dp83848_Object_t *pObj);
int32_t DP83848_GetLinkState(dp83848_Object_t *pObj);
int32_t DP83848_SetLinkState(dp83848_Object_t *pObj, uint32_t LinkState);
int32_t DP83848_EnableLoopbackMode(dp83848_Object_t *pObj);
int32_t DP83848_DisableLoopbackMode(dp83848_Object_t *pObj);

#ifdef __cplusplus
}
#endif

#endif
