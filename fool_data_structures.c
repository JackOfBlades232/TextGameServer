/* TextGameServer/fool_data_structures.c */
#include "utils.h"

#define NUM_SUITS            4
#define NUM_VALS             13

#define BASE_PLAYER_CARDS    6
#define MAX_TABLE_CARDS      BASE_PLAYER_CARDS
#define MIN_PLAYERS_PER_GAME 2
#define DECK_SIZE            NUM_SUITS*NUM_VALS
#define MAX_PLAYERS_PER_GAME DECK_SIZE/BASE_PLAYER_CARDS

typedef enum card_suit_tag {
    cs_empty     = 0, 
    cs_spades    = 1, 
    cs_clubs     = 2,
    cs_hearts    = 3,
    cs_diamonds  = 4
} card_suit_t;

typedef enum card_value_tag {
    cv_empty  = 0,
    cv_two    = 2,
    cv_three  = 3,
    cv_four   = 4,
    cv_five   = 5,
    cv_six    = 6,
    cv_seven  = 7,
    cv_eight  = 8,
    cv_nine   = 9,
    cv_ten    = 10,
    cv_jack   = 11,
    cv_queen  = 12,
    cv_king   = 13,
    cv_ace    = 14,
} card_value_t;

typedef struct card_tag {
    card_suit_t suit;
    card_value_t val;
} card_t;

typedef struct deck_tag {
    card_t cards[DECK_SIZE];
    card_t trump;
    card_t *head;
} deck_t;

typedef struct table_tag {
    card_t *faceoffs[MAX_TABLE_CARDS][2];
    int cards_played;
    int cards_beat;
} table_t;

typedef enum player_state_tag {
    ps_waiting,
    ps_attacking,
    ps_defending,
    ps_spectating
} player_state_t;

typedef enum game_state_tag {
    gs_awaiting_players,
    gs_first_card,
    gs_free_for_all,
    gs_game_end     
} game_state_t;

static inline char card_char_index(int i)
{
    return i < 'z' - 'a' ? i + 'a' : i + 'A' - ('z' - 'a');
}

static inline int card_char_index_to_int(char c)
{
    return c <= 'z' ? c - 'a' : c - 'A' + ('z' - 'a');
}

static inline bool deck_size(deck_t *d)
{
    return d->head - d->cards + 1;
}

static inline bool deck_is_empty(deck_t *d)
{
    return deck_size(d) <= 0;
}

static inline bool table_is_beaten(table_t *t)
{
    return t->cards_played <= t->cards_beat;
}

static inline bool table_is_full(table_t *t, linked_list_t *defender_hand)
{
    return t->cards_played >= MIN(defender_hand->size + t->cards_beat, MAX_TABLE_CARDS);
}

static void generate_deck(deck_t *d)
{
    for (int i = 0; i < DECK_SIZE; i++) {
        d->cards[i].suit = i % NUM_SUITS + cs_spades;
        d->cards[i].val = (i / NUM_SUITS) + cv_two;
    }

    DO_RANDOM_PERMUTATION(card_t, d->cards, DECK_SIZE);

    d->head = &d->cards[DECK_SIZE-1];
    d->trump = d->cards[0];
}

static void reset_table(table_t *t)
{
    for (int i = 0; i < MAX_TABLE_CARDS; i++) {
        t->faceoffs[i][0] = NULL;
        t->faceoffs[i][1] = NULL;
    }

    t->cards_played = 0;
    t->cards_beat = 0;
}

static void flush_table(table_t *t, linked_list_t *hand)
{
    for (int i = 0; i < t->cards_played; i++) {
        ll_push_front(hand, t->faceoffs[i][0]);
        if (i < t->cards_beat)
            ll_push_front(hand, t->faceoffs[i][1]);
    }

    reset_table(t);
}

static card_t *pop_card_from_deck(deck_t *d)
{
    if (deck_is_empty(d))
        return NULL;

    card_t *ret = d->head;
    d->head--;
    return ret;
}

static bool attacker_can_play_card(table_t *t, card_t c)
{
    if (t->cards_played <= 0)
        return true;
    else if (t->cards_played >= MAX_TABLE_CARDS)
        return false;

    for (int i = 0; i < t->cards_played; i++) {
        if (
                t->faceoffs[i][0]->val == c.val ||
                (i < t->cards_beat && t->faceoffs[i][1]->val == c.val)
           ) 
        {
            return true;
        }
    }

    return false;
}

static inline bool card_is_less(card_t c1, card_t c2, card_suit_t trump_suit)
{
    return (c1.suit == c2.suit && c1.val < c2.val) ||
           (c1.suit != trump_suit && c2.suit == trump_suit);
}

static bool defender_can_play_card(table_t *t, card_t c, card_suit_t trump_suit)
{
    if (t->cards_played <= 0 || t->cards_beat >= t->cards_played)
        return false;

    card_t *faceoff_card = t->faceoffs[t->cards_beat][0];
    return card_is_less(*faceoff_card, c, trump_suit);
}

static bool attacker_try_play_card(table_t *t, card_t *c)
{
    if (!attacker_can_play_card(t, *c))
        return false;

    t->faceoffs[t->cards_played++][0] = c;
    return true;
}

static bool defender_try_play_card(table_t *t, card_t *c, card_suit_t trump_suit)
{
    if (!defender_can_play_card(t, *c, trump_suit))
        return false;

    t->faceoffs[t->cards_beat++][1] = c;
    return true;
}

static bool player_can_attack(table_t *t, linked_list_t *hand)
{
    list_node_t *node = hand->head;
    while (node) {
        card_t *card = node->data;
        if (attacker_can_play_card(t, *card))
            return true;

        node = node->next;
    }

    return false;
}
