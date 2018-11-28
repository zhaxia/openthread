#ifndef NRF_COEX_API_H_
#define NRF_COEX_API_H_

#include <stdbool.h>
#include <stdint.h>

#include "nrf_802154_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void nrf_coex_init(void);
void nrf_coex_start(void);
void nrf_coex_stop(void);
bool nrf_coex_rx_request(void);
bool nrf_coex_tx_request(const uint8_t *p_data);
bool nrf_coex_grant_active(void);
void nrf_coex_critical_section_enter(void);
void nrf_coex_critical_section_exit(void);
void nrf_coex_rx_ended_hook(bool success);
void nrf_coex_tx_ended_hook(bool success);

#ifdef __cplusplus
}
#endif

#endif /* NRF_COEX_API_H_ */
