/**
 * @file cart_bus.c
 * @brief Gestion du bus de communication série entre Brick et les cartouches XVA.
 *
 * Ce module gère la file d’envoi asynchrone (mailbox + pool mémoire) pour chaque
 * cartouche connectée sur un port UART.
 * Il assure :
 * - La configuration des UARTs (CART1 à CART4)
 * - La mise en file des commandes SET/GET de paramètres
 * - L’encapsulation des messages selon le protocole `cart_proto_xva1`
 * - La transmission séquencée par thread dédié (un par cartouche)
 *
 * @details
 * Chaque port de cartouche dispose d’une mailbox (`chMB`) et d’un pool de commandes.
 * Les requêtes sont postées via `cart_set_param()` ou `cart_get_param()`, puis
 * envoyées par le thread `cart_tx_thread`.
 *
 * UART mapping :
 * | Cart | UART   | Broches STM32  |
 * |------|--------|----------------|
 * | 1    | USART1 | PA9 / PA10     |
 * | 2    | UART4  | PE8 / PE7      |
 * | 3    | USART3 | PB10 / PB11    |
 * | 4    | USART2 | PA2 / PA3      |
 *
 * @ingroup cart
 */

#include "ch.h"
#include "hal.h"
#include "cart_bus.h"
#include "cart_proto.h"
#if CH_CFG_USE_REGISTRY
#include "chprintf.h"    /* chsnprintf */
#endif
/* ===========================================================
 * ⚙️ Configuration
 * =========================================================== */
#ifndef CART_QUEUE_LEN
#define CART_QUEUE_LEN 64
#endif
#ifndef CART_MB_DROP_OLDEST
#define CART_MB_DROP_OLDEST 1
#endif
#ifndef CART_UART_BAUD
#define CART_UART_BAUD 500000u   /* XVA1 = 500 kbaud, 8N1 */
#endif
#ifndef CART_TX_THREAD_PRIO
         #define CART_TX_THREAD_PRIO (NORMALPRIO + 2)
         #endif
/* ===========================================================
 * Structures internes
 * =========================================================== */
typedef struct {
    uint16_t param;
    uint8_t  value;
    bool     is_get;
} cart_cmd_t;

typedef struct {
    SerialDriver *uart;
    mailbox_t     mb;
    msg_t         q[CART_QUEUE_LEN];
    memory_pool_t pool;
    cart_cmd_t    pool_buf[CART_QUEUE_LEN];
    thread_t     *tx;
} cart_port_t;

/* ===========================================================
 * Variables globales
 * =========================================================== */
static cart_port_t s_port[CART_COUNT];
cart_tx_stats_t    cart_stats[CART_COUNT];

/* ===========================================================
 * 🧭 Mapping logique → UART physique
 * =========================================================== */
static SerialDriver* map_uart(cart_id_t id) {
    switch (id) {
        case CART1: return &SD1;
        case CART2: return &SD4;
        case CART3: return &SD3;
        case CART4: return &SD2;
        default:    return NULL;
    }
}

/* ===========================================================
 * Thread d’émission
 * =========================================================== */
static THD_WORKING_AREA(waCartTx[CART_COUNT], 512);

static THD_FUNCTION(cart_tx_thread, arg) {
    const cart_id_t id = (cart_id_t)(uintptr_t)arg;
    cart_port_t *p = &s_port[id];
    uint8_t frame[4];

#if CH_CFG_USE_REGISTRY
    char name[16];
    chsnprintf(name, sizeof(name), "cart_tx_%d", (int)id + 1);
    chRegSetThreadName(name);
#endif

    while (true) {
        cart_cmd_t *cmd;
        chMBFetchTimeout(&p->mb, (msg_t*)&cmd, TIME_INFINITE);

        const size_t len = cmd->is_get
            ? cart_proto_build_get(cmd->param, frame)
            : cart_proto_build_set(cmd->param, cmd->value, frame);

        sdWrite(p->uart, frame, len);
        cart_stats[id].tx_sent++;

        chPoolFree(&p->pool, cmd);
    }
}

/* ===========================================================
 * Initialisation du bus cartouche
 * =========================================================== */
void cart_bus_init(void) {
    const SerialConfig cfg = { CART_UART_BAUD, 0, 0, 0 };

    for (int i = 0; i < CART_COUNT; i++) {
        cart_port_t *p = &s_port[i];
        p->uart = map_uart((cart_id_t)i);
        chDbgAssert(p->uart != NULL, "UART map invalid");
        sdStart(p->uart, &cfg);

        chMBObjectInit(&p->mb, p->q, CART_QUEUE_LEN);
        chPoolObjectInit(&p->pool, sizeof(cart_cmd_t), NULL);
        chPoolLoadArray(&p->pool, p->pool_buf, CART_QUEUE_LEN);

        cart_stats[i].tx_sent = 0;
        cart_stats[i].tx_dropped = 0;
        cart_stats[i].mb_full = 0;


         p->tx = chThdCreateStatic(waCartTx[i], sizeof(waCartTx[i]),
                                   CART_TX_THREAD_PRIO, cart_tx_thread, (void*)(uintptr_t)i);
        chDbgAssert(p->tx != NULL, "cart_tx thd fail");
    }
}

/* ===========================================================
 * Commandes internes (SET/GET)
 * =========================================================== */
static bool post_cmd(cart_id_t id, bool is_get, uint16_t param, uint8_t value) {
    if (id >= CART_COUNT) return false;
    cart_port_t *p = &s_port[id];

    cart_cmd_t *cmd = (cart_cmd_t*)chPoolAlloc(&p->pool);
    if (!cmd) { cart_stats[id].tx_dropped++; return false; }

    cmd->param = param;
    cmd->value = value;
    cmd->is_get = is_get;

    if (chMBPostTimeout(&p->mb, (msg_t)cmd, TIME_IMMEDIATE) != MSG_OK) {
        cart_stats[id].mb_full++;
#if CART_MB_DROP_OLDEST
        msg_t old;
        if (chMBFetchTimeout(&p->mb, &old, TIME_IMMEDIATE) == MSG_OK) {
            chPoolFree(&p->pool, (void*)old);
            (void)chMBPostTimeout(&p->mb, (msg_t)cmd, TIME_IMMEDIATE);
        } else {
            chPoolFree(&p->pool, cmd);
            return false;
        }
#else
        chPoolFree(&p->pool, cmd);
        return false;
#endif
    }
    return true;
}

/* ===========================================================
 * API publique
 * =========================================================== */
bool cart_set_param(cart_id_t id, uint16_t param, uint8_t value) {
    return post_cmd(id, false, param, value);
}

bool cart_get_param(cart_id_t id, uint16_t param) {
    return post_cmd(id, true, param, 0);
}
