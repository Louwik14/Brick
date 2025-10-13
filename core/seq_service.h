/**
 * @file seq_service.h
 * @brief High level bootstrap for the sequencer runtime/engine duo.
 * @ingroup core
 */
#ifndef BRICK_SEQ_SERVICE_H
#define BRICK_SEQ_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise sequencer runtime structures and start the engine. */
void seq_service_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_SEQ_SERVICE_H */
