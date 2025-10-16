/**
 * @file drv_pots.c
 * @brief Driver pour potentiomètres analogiques (ADC1, canaux IN10–IN13).
 *
 * Ce module lit en continu les potentiomètres connectés aux entrées analogiques
 * du STM32 via **ADC1**, avec moyennage automatique sur plusieurs échantillons.
 *
 * Fonctionnalités :
 * - Acquisition simultanée des 4 voies PC0–PC3 (IN10–IN13)
 * - Moyennage logiciel sur 8 échantillons
 * - Lecture périodique dans un thread dédié (~50 Hz)
 *
 * @ingroup drivers
 */

#include "drv_pots.h"
#include "brick_config.h"

/* =======================================================================
 *                              CONSTANTES
 * ======================================================================= */

#define ADC_GRP_NUM_CHANNELS   NUM_POTS      /**< Nombre de canaux ADC utilisés */
#define ADC_GRP_BUF_DEPTH      8             /**< Taille du buffer pour le moyennage */

/* =======================================================================
 *                              VARIABLES INTERNES
 * ======================================================================= */

static adcsample_t samples[ADC_GRP_NUM_CHANNELS * ADC_GRP_BUF_DEPTH]; /**< Buffer brut ADC */
static int pot_values[NUM_POTS];                                      /**< Valeurs moyennées */

/* =======================================================================
 *                              CONFIGURATION ADC
 * ======================================================================= */

/**
 * @brief Groupe de conversion ADC configuré pour lecture continue des 4 potentiomètres.
 */
static const ADCConversionGroup adcgrpcfg = {
    TRUE,                       /**< Mode circulaire */
    ADC_GRP_NUM_CHANNELS,       /**< Nombre de canaux */
    NULL, NULL,                 /**< Pas de callback */
    0,                          /**< CR1 */
    ADC_CR2_SWSTART,            /**< CR2 */
    ADC_SMPR1_SMP_AN10(ADC_SAMPLE_56) |
    ADC_SMPR1_SMP_AN11(ADC_SAMPLE_56) |
    ADC_SMPR1_SMP_AN12(ADC_SAMPLE_56) |
    ADC_SMPR1_SMP_AN13(ADC_SAMPLE_56), /**< Temps d’échantillonnage */
    0,                          /**< SMPR2 */
    0,0,0,0,                    /**< HTR, LTR, SQR1, SQR2 */
    ADC_SQR3_SQ1_N(10) |        /**< PC0 → IN10 */
    ADC_SQR3_SQ2_N(11) |        /**< PC1 → IN11 */
    ADC_SQR3_SQ3_N(12) |        /**< PC2 → IN12 */
    ADC_SQR3_SQ4_N(13)          /**< PC3 → IN13 */
};

/* =======================================================================
 *                              THREAD DE LECTURE
 * ======================================================================= */

/**
 * @brief Thread chargé de la conversion ADC et du moyennage des potentiomètres.
 *
 * - Démarre la conversion continue sur ADC1
 * - Calcule la moyenne glissante sur 8 échantillons
 * - Met à jour `pot_values[]` toutes les 20 ms (~50 Hz)
 */
static CCM_DATA THD_WORKING_AREA(waPotReader, 256);
static THD_FUNCTION(potReaderThread, arg) {
    (void)arg;
    chRegSetThreadName("PotReader");

    adcStart(&ADCD1, NULL);
    adcStartConversion(&ADCD1, &adcgrpcfg, samples, ADC_GRP_BUF_DEPTH);

    while (true) {
        for (int ch = 0; ch < NUM_POTS; ch++) {
            uint32_t sum = 0;
            for (int i = 0; i < ADC_GRP_BUF_DEPTH; i++) {
                sum += samples[ch + i * ADC_GRP_NUM_CHANNELS];
            }
            pot_values[ch] = (int)(sum / ADC_GRP_BUF_DEPTH);
        }
        chThdSleepMilliseconds(20);
    }
}

/* =======================================================================
 *                              API PUBLIQUE
 * ======================================================================= */

/**
 * @brief Initialise le driver des potentiomètres (configuration GPIO optionnelle).
 */
void drv_pots_init(void) {
    /* Optionnel : configuration des GPIOs PC0–PC3 en mode analogique */
}

/**
 * @brief Démarre le thread de lecture des potentiomètres.
 */
void drv_pots_start(void) {
    chThdCreateStatic(waPotReader, sizeof(waPotReader), NORMALPRIO, potReaderThread, NULL);
}

/**
 * @brief Retourne la valeur moyenne actuelle d’un potentiomètre.
 * @param index Indice du potentiomètre [0–NUM_POTS-1].
 * @return Valeur ADC moyenne (0–4095).
 */
int drv_pots_get(int index) {
    if (index < 0 || index >= NUM_POTS) return 0;
    return pot_values[index];
}
