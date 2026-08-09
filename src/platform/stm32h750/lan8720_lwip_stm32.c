#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "platform/stm32h750_bringup.h"

#include "ethernetif.h"
#include "lwip/ip4_addr.h"
#include "lwip/timeouts.h"

static Lan8720Result lwip_init_port(void *ctx, const Lan8720Config *config) {
  Stm32Lan8720Context *lan = (Stm32Lan8720Context *)ctx;
  ip4_addr_t ip;
  ip4_addr_t netmask;
  ip4_addr_t gateway;
  if (lan == NULL || lan->netif == NULL || config == NULL) {
    return LAN8720_ERROR;
  }

  IP4_ADDR(&ip,
           (uint8_t)(config->ip >> 24u),
           (uint8_t)(config->ip >> 16u),
           (uint8_t)(config->ip >> 8u),
           (uint8_t)config->ip);
  IP4_ADDR(&netmask,
           (uint8_t)(config->netmask >> 24u),
           (uint8_t)(config->netmask >> 16u),
           (uint8_t)(config->netmask >> 8u),
           (uint8_t)config->netmask);
  IP4_ADDR(&gateway,
           (uint8_t)(config->gateway >> 24u),
           (uint8_t)(config->gateway >> 16u),
           (uint8_t)(config->gateway >> 8u),
           (uint8_t)config->gateway);
  netif_set_addr(lan->netif, &ip, &netmask, &gateway);
  return LAN8720_OK;
}

static Lan8720Result lwip_start(void *ctx) {
  Stm32Lan8720Context *lan = (Stm32Lan8720Context *)ctx;
  if (lan == NULL || lan->netif == NULL) {
    return LAN8720_ERROR;
  }
  netif_set_default(lan->netif);
  netif_set_up(lan->netif);
  return LAN8720_OK;
}

static Lan8720Result lwip_poll(void *ctx) {
  Stm32Lan8720Context *lan = (Stm32Lan8720Context *)ctx;
  if (lan == NULL || lan->netif == NULL) {
    return LAN8720_ERROR;
  }
  ethernet_link_check_state(lan->netif);
  (void)ethernetif_input(lan->netif);
  sys_check_timeouts();
  return LAN8720_OK;
}

static bool lwip_link_up(void *ctx) {
  Stm32Lan8720Context *lan = (Stm32Lan8720Context *)ctx;
  return lan != NULL && lan->netif != NULL && netif_is_link_up(lan->netif);
}

static uint32_t lwip_get_ip(void *ctx) {
  Stm32Lan8720Context *lan = (Stm32Lan8720Context *)ctx;
  if (lan == NULL || lan->netif == NULL) {
    return 0u;
  }
  return lan->netif->ip_addr.addr;
}

void stm32h750_lan8720_bind(Lan8720Port *port, Stm32Lan8720Context *ctx, struct netif *netif) {
  static const Lan8720PortOps ops = {
    .init = lwip_init_port,
    .start = lwip_start,
    .poll = lwip_poll,
    .link_up = lwip_link_up,
    .get_ip = lwip_get_ip,
  };
  if (ctx == NULL) {
    lan8720_port_bind(port, NULL, &ops);
    return;
  }
  ctx->netif = netif;
  lan8720_port_bind(port, ctx, &ops);
}

#endif
