#include "mgos.h"
#include "mgos_mongoose.h"
#include "mgos_wifi.h"
#include "mgos_dns_sd.h"

static void timer_cb(void *arg) {
  mgos_gpio_toggle(mgos_sys_config_get_board_led1_pin());
  LOG(LL_INFO,
      ("uptime: %.2lf, RAM: %lu, %lu free",
       mgos_uptime(), (unsigned long) mgos_get_heap_size(),
       (unsigned long) mgos_get_free_heap_size()));
  (void) arg;
}

// Define an event handler function
static void ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  const int uart_no = mgos_sys_config_get_sb_uart_no();
  struct mbuf *io = &nc->recv_mbuf;

  switch (ev) {
    case MG_EV_ACCEPT:
      nc->flags |= MG_F_USER_1;// mark connection
      break;
    case MG_EV_RECV:
      mgos_uart_write(uart_no, io->buf, io->len);
      mbuf_remove(io, io->len);
      break;
    default:
      break;
  }

  (void)ev_data;
  (void)user_data;
}

static void uart_dispatcher(int uart_no, void *arg) {
  assert(uart_no == mgos_sys_config_get_sb_uart_no());

  if (mgos_uart_read_avail(uart_no) == 0) return;

  uint8_t buf[128];
  size_t n = mgos_uart_read(uart_no, (void*)buf, 128);

  struct mg_connection *c;
  for (c = mg_next(mgos_get_mgr(), NULL); c != NULL; c = mg_next(mgos_get_mgr(), c)) {
    if ((c->flags & MG_F_USER_1) == 0) continue; //skip unmarked connections
    
    mg_send(c, buf, n);
  }
  
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  const int uart_no = mgos_sys_config_get_sb_uart_no();
  struct mgos_uart_config ucfg;
  mgos_uart_config_set_defaults(uart_no, &ucfg);

  ucfg.baud_rate = 9600;

  if (!mgos_uart_configure(uart_no, &ucfg)) {
    LOG(LL_ERROR, ("Failed to configure UART%d", uart_no));
  }

  mgos_gpio_set_mode(mgos_sys_config_get_board_led1_pin(), MGOS_GPIO_MODE_OUTPUT);
  mgos_set_timer(1000, MGOS_TIMER_REPEAT, timer_cb, NULL);

  char address[32];
  snprintf(address, sizeof(address), ":%d", mgos_sys_config_get_sb_port());
  mg_bind(mgos_get_mgr(), address, ev_handler, 0);

  mgos_uart_set_dispatcher(uart_no, uart_dispatcher, 0);
  mgos_uart_set_rx_enabled(uart_no, true);

  return MGOS_APP_INIT_SUCCESS;
}
