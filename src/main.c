#include "mgos.h"
#include "mgos_mongoose.h"
#include "mgos_wifi.h"

static void timer_cb(void *arg) {
  static bool s_tick_tock = false;
  LOG(LL_INFO,
      ("%s uptime: %.2lf, RAM: %lu, %lu free", (s_tick_tock ? "Tick" : "Tock"),
       mgos_uptime(), (unsigned long) mgos_get_heap_size(),
       (unsigned long) mgos_get_free_heap_size()));
  s_tick_tock = !s_tick_tock;
  (void) arg;
}

static void net_cb(int ev, void *evd, void *arg) {
  switch (ev) {
    case MGOS_NET_EV_DISCONNECTED:
      LOG(LL_INFO, ("%s", "Net disconnected"));
      break;
    case MGOS_NET_EV_CONNECTING:
      LOG(LL_INFO, ("%s", "Net connecting..."));
      break;
    case MGOS_NET_EV_CONNECTED:
      LOG(LL_INFO, ("%s", "Net connected"));
      break;
    case MGOS_NET_EV_IP_ACQUIRED:
      LOG(LL_INFO, ("%s", "Net got IP address"));
      break;
  }

  (void) evd;
  (void) arg;
}

static void wifi_cb(int ev, void *evd, void *arg) {
  switch (ev) {
    case MGOS_WIFI_EV_STA_DISCONNECTED: {
      struct mgos_wifi_sta_disconnected_arg *da =
          (struct mgos_wifi_sta_disconnected_arg *) evd;
      LOG(LL_INFO, ("WiFi STA disconnected, reason %d", da->reason));
      break;
    }
    case MGOS_WIFI_EV_STA_CONNECTING:
      LOG(LL_INFO, ("WiFi STA connecting %p", arg));
      break;
    case MGOS_WIFI_EV_STA_CONNECTED:
      LOG(LL_INFO, ("WiFi STA connected %p", arg));
      break;
    case MGOS_WIFI_EV_STA_IP_ACQUIRED:
      LOG(LL_INFO, ("WiFi STA IP acquired %p", arg));
      break;
    case MGOS_WIFI_EV_AP_STA_CONNECTED: {
      struct mgos_wifi_ap_sta_connected_arg *aa =
          (struct mgos_wifi_ap_sta_connected_arg *) evd;
      LOG(LL_INFO, ("WiFi AP STA connected MAC %02x:%02x:%02x:%02x:%02x:%02x",
                    aa->mac[0], aa->mac[1], aa->mac[2], aa->mac[3], aa->mac[4],
                    aa->mac[5]));
      break;
    }
    case MGOS_WIFI_EV_AP_STA_DISCONNECTED: {
      struct mgos_wifi_ap_sta_disconnected_arg *aa =
          (struct mgos_wifi_ap_sta_disconnected_arg *) evd;
      LOG(LL_INFO,
          ("WiFi AP STA disconnected MAC %02x:%02x:%02x:%02x:%02x:%02x",
           aa->mac[0], aa->mac[1], aa->mac[2], aa->mac[3], aa->mac[4],
           aa->mac[5]));
      break;
    }
  }
  (void) arg;
}

// Define an event handler function
static void ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  struct mbuf *io = &nc->recv_mbuf;

  switch (ev) {
    case MG_EV_ACCEPT:
      nc->flags |= MG_F_USER_1;// mark connection
      break;
    case MG_EV_RECV:
      mgos_uart_write(1, io->buf, io->len);
      mbuf_remove(io, io->len);
      break;
    default:
      break;
  }

  (void)ev_data;
  (void)user_data;
}

static void uart_dispatcher(int uart_no, void *arg) {
  assert(uart_no == 1);

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
  int uart_no = 1;

  struct mgos_uart_config ucfg;
  mgos_uart_config_set_defaults(uart_no, &ucfg);

  ucfg.baud_rate = 9600;

  if (!mgos_uart_configure(uart_no, &ucfg)) {
    LOG(LL_ERROR, ("Failed to configure UART%d", uart_no));
  }

  /* Simple repeating timer */
  mgos_set_timer(1000, MGOS_TIMER_REPEAT, timer_cb, NULL);

  /* Network connectivity events */
  mgos_event_add_group_handler(MGOS_EVENT_GRP_NET, net_cb, NULL);

  mgos_event_add_group_handler(MGOS_WIFI_EV_BASE, wifi_cb, NULL);

  mg_bind(mgos_get_mgr(), ":1234", ev_handler, 0);

  mgos_uart_set_dispatcher(1, uart_dispatcher, 0);
  mgos_uart_set_rx_enabled(1, true);

  return MGOS_APP_INIT_SUCCESS;
}
