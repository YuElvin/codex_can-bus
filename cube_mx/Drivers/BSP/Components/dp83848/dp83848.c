#include "dp83848.h"

#define DP83848_FIRST_DEV_ADDR ((uint32_t)0U)
#define DP83848_MAX_DEV_ADDR   ((uint32_t)31U)

static int32_t DP83848_IsValidPhyId(uint32_t id1, uint32_t id2)
{
  return id1 != 0U && id1 != 0xFFFFU && id2 != 0U && id2 != 0xFFFFU;
}

int32_t DP83848_RegisterBusIO(dp83848_Object_t *pObj, dp83848_IOCtx_t *ioctx)
{
  if (!pObj || !ioctx || !ioctx->ReadReg || !ioctx->WriteReg || !ioctx->GetTick)
  {
    return DP83848_STATUS_ERROR;
  }

  pObj->IO.Init = ioctx->Init;
  pObj->IO.DeInit = ioctx->DeInit;
  pObj->IO.ReadReg = ioctx->ReadReg;
  pObj->IO.WriteReg = ioctx->WriteReg;
  pObj->IO.GetTick = ioctx->GetTick;

  return DP83848_STATUS_OK;
}

int32_t DP83848_Init(dp83848_Object_t *pObj)
{
  uint32_t id1 = 0U;
  uint32_t id2 = 0U;
  uint32_t phycr = 0U;
  int32_t status = DP83848_STATUS_OK;

  if (pObj == 0)
  {
    return DP83848_STATUS_ERROR;
  }

  if (pObj->Is_Initialized == 0U)
  {
    if (pObj->IO.Init != 0)
    {
      (void)pObj->IO.Init();
    }

    pObj->DevAddr = DP83848_MAX_DEV_ADDR + 1U;

    for (uint32_t addr = DP83848_FIRST_DEV_ADDR; addr <= DP83848_MAX_DEV_ADDR; addr++)
    {
      if (pObj->IO.ReadReg(addr, DP83848_PHYIDR1, &id1) < 0)
      {
        status = DP83848_STATUS_READ_ERROR;
        continue;
      }
      if (pObj->IO.ReadReg(addr, DP83848_PHYIDR2, &id2) < 0)
      {
        status = DP83848_STATUS_READ_ERROR;
        continue;
      }
      if (DP83848_IsValidPhyId(id1, id2) == 0)
      {
        continue;
      }
      if (pObj->IO.ReadReg(addr, DP83848_PHYCR, &phycr) < 0)
      {
        status = DP83848_STATUS_READ_ERROR;
        continue;
      }
      if ((phycr & DP83848_PHYCR_PHY_ADDR) == addr)
      {
        pObj->DevAddr = addr;
        status = DP83848_STATUS_OK;
        break;
      }
    }

    if (pObj->DevAddr > DP83848_MAX_DEV_ADDR)
    {
      status = DP83848_STATUS_ADDRESS_ERROR;
    }

    if (status == DP83848_STATUS_OK)
    {
      (void)DP83848_DisablePowerDownMode(pObj);
      pObj->Is_Initialized = 1U;
    }
  }

  return status;
}

int32_t DP83848_DeInit(dp83848_Object_t *pObj)
{
  if (pObj != 0 && pObj->Is_Initialized != 0U)
  {
    if (pObj->IO.DeInit != 0 && pObj->IO.DeInit() < 0)
    {
      return DP83848_STATUS_ERROR;
    }
    pObj->Is_Initialized = 0U;
  }

  return DP83848_STATUS_OK;
}

int32_t DP83848_DisablePowerDownMode(dp83848_Object_t *pObj)
{
  uint32_t readval = 0U;

  if (pObj->IO.ReadReg(pObj->DevAddr, DP83848_BMCR, &readval) < 0)
  {
    return DP83848_STATUS_READ_ERROR;
  }
  readval &= ~(DP83848_BMCR_POWER_DOWN | DP83848_BMCR_ISOLATE);
  if (pObj->IO.WriteReg(pObj->DevAddr, DP83848_BMCR, readval) < 0)
  {
    return DP83848_STATUS_WRITE_ERROR;
  }

  return DP83848_STATUS_OK;
}

int32_t DP83848_EnablePowerDownMode(dp83848_Object_t *pObj)
{
  uint32_t readval = 0U;

  if (pObj->IO.ReadReg(pObj->DevAddr, DP83848_BMCR, &readval) < 0)
  {
    return DP83848_STATUS_READ_ERROR;
  }
  readval |= DP83848_BMCR_POWER_DOWN;
  if (pObj->IO.WriteReg(pObj->DevAddr, DP83848_BMCR, readval) < 0)
  {
    return DP83848_STATUS_WRITE_ERROR;
  }

  return DP83848_STATUS_OK;
}

int32_t DP83848_StartAutoNego(dp83848_Object_t *pObj)
{
  uint32_t readval = 0U;

  if (pObj->IO.ReadReg(pObj->DevAddr, DP83848_BMCR, &readval) < 0)
  {
    return DP83848_STATUS_READ_ERROR;
  }
  readval |= DP83848_BMCR_AUTONEGO_EN | DP83848_BMCR_RESTART_AUTONEGO;
  if (pObj->IO.WriteReg(pObj->DevAddr, DP83848_BMCR, readval) < 0)
  {
    return DP83848_STATUS_WRITE_ERROR;
  }

  return DP83848_STATUS_OK;
}

int32_t DP83848_GetLinkState(dp83848_Object_t *pObj)
{
  uint32_t bmsr = 0U;
  uint32_t bmcr = 0U;
  uint32_t physts = 0U;

  if (pObj == 0 || pObj->DevAddr > DP83848_MAX_DEV_ADDR)
  {
    return DP83848_STATUS_ADDRESS_ERROR;
  }

  if (pObj->IO.ReadReg(pObj->DevAddr, DP83848_BMSR, &bmsr) < 0)
  {
    return DP83848_STATUS_READ_ERROR;
  }
  if (pObj->IO.ReadReg(pObj->DevAddr, DP83848_BMSR, &bmsr) < 0)
  {
    return DP83848_STATUS_READ_ERROR;
  }
  if ((bmsr & DP83848_BMSR_LINK_STATUS) == 0U)
  {
    return DP83848_STATUS_LINK_DOWN;
  }

  if (pObj->IO.ReadReg(pObj->DevAddr, DP83848_BMCR, &bmcr) < 0)
  {
    return DP83848_STATUS_READ_ERROR;
  }
  if ((bmcr & DP83848_BMCR_AUTONEGO_EN) == 0U)
  {
    if ((bmcr & DP83848_BMCR_SPEED_SELECT) != 0U)
    {
      return (bmcr & DP83848_BMCR_DUPLEX_MODE) != 0U ?
        DP83848_STATUS_100MBITS_FULLDUPLEX :
        DP83848_STATUS_100MBITS_HALFDUPLEX;
    }
    return (bmcr & DP83848_BMCR_DUPLEX_MODE) != 0U ?
      DP83848_STATUS_10MBITS_FULLDUPLEX :
      DP83848_STATUS_10MBITS_HALFDUPLEX;
  }

  if (pObj->IO.ReadReg(pObj->DevAddr, DP83848_PHYSTS, &physts) < 0)
  {
    return DP83848_STATUS_READ_ERROR;
  }
  if ((physts & DP83848_PHYSTS_LINK_STATUS) == 0U)
  {
    return DP83848_STATUS_LINK_DOWN;
  }
  if ((physts & DP83848_PHYSTS_AUTONEGO_DONE) == 0U)
  {
    return DP83848_STATUS_AUTONEGO_NOTDONE;
  }

  if ((physts & DP83848_PHYSTS_SPEED_10M) == 0U)
  {
    return (physts & DP83848_PHYSTS_DUPLEX_STATUS) != 0U ?
      DP83848_STATUS_100MBITS_FULLDUPLEX :
      DP83848_STATUS_100MBITS_HALFDUPLEX;
  }
  return (physts & DP83848_PHYSTS_DUPLEX_STATUS) != 0U ?
    DP83848_STATUS_10MBITS_FULLDUPLEX :
    DP83848_STATUS_10MBITS_HALFDUPLEX;
}

int32_t DP83848_SetLinkState(dp83848_Object_t *pObj, uint32_t LinkState)
{
  uint32_t bmcr = 0U;

  if (pObj->IO.ReadReg(pObj->DevAddr, DP83848_BMCR, &bmcr) < 0)
  {
    return DP83848_STATUS_READ_ERROR;
  }

  bmcr &= ~(DP83848_BMCR_AUTONEGO_EN | DP83848_BMCR_SPEED_SELECT | DP83848_BMCR_DUPLEX_MODE);
  if (LinkState == DP83848_STATUS_100MBITS_FULLDUPLEX)
  {
    bmcr |= DP83848_BMCR_SPEED_SELECT | DP83848_BMCR_DUPLEX_MODE;
  }
  else if (LinkState == DP83848_STATUS_100MBITS_HALFDUPLEX)
  {
    bmcr |= DP83848_BMCR_SPEED_SELECT;
  }
  else if (LinkState == DP83848_STATUS_10MBITS_FULLDUPLEX)
  {
    bmcr |= DP83848_BMCR_DUPLEX_MODE;
  }
  else if (LinkState != DP83848_STATUS_10MBITS_HALFDUPLEX)
  {
    return DP83848_STATUS_ERROR;
  }

  if (pObj->IO.WriteReg(pObj->DevAddr, DP83848_BMCR, bmcr) < 0)
  {
    return DP83848_STATUS_WRITE_ERROR;
  }

  return DP83848_STATUS_OK;
}

int32_t DP83848_EnableLoopbackMode(dp83848_Object_t *pObj)
{
  uint32_t bmcr = 0U;

  if (pObj->IO.ReadReg(pObj->DevAddr, DP83848_BMCR, &bmcr) < 0)
  {
    return DP83848_STATUS_READ_ERROR;
  }
  bmcr |= DP83848_BMCR_LOOPBACK;
  if (pObj->IO.WriteReg(pObj->DevAddr, DP83848_BMCR, bmcr) < 0)
  {
    return DP83848_STATUS_WRITE_ERROR;
  }

  return DP83848_STATUS_OK;
}

int32_t DP83848_DisableLoopbackMode(dp83848_Object_t *pObj)
{
  uint32_t bmcr = 0U;

  if (pObj->IO.ReadReg(pObj->DevAddr, DP83848_BMCR, &bmcr) < 0)
  {
    return DP83848_STATUS_READ_ERROR;
  }
  bmcr &= ~DP83848_BMCR_LOOPBACK;
  if (pObj->IO.WriteReg(pObj->DevAddr, DP83848_BMCR, bmcr) < 0)
  {
    return DP83848_STATUS_WRITE_ERROR;
  }

  return DP83848_STATUS_OK;
}
