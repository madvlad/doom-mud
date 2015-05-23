/* ************************************************************************
*   File: class.c                                       Part of CircleMUD *
*  Usage: Source file for class-specific code                             *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/*
 * This file attempts to concentrate most of the code which must be changed
 * in order for new classes to be added.  If you're adding a new class,
 * you should go through this entire file from beginning to end and add
 * the appropriate new special cases for your new class.
 */



#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "db.h"
#include "utils.h"
#include "spells.h"
#include "interpreter.h"
#include "constants.h"

/* local functions */
void snoop_check(struct char_data *ch);
int parse_class(char arg);
bitvector_t find_class_bitvector(const char *arg);
byte saving_throws(int class_num, int type, int level);
int thaco(struct char_data *ch);
void roll_real_abils(struct char_data *ch);
void do_start(struct char_data *ch);
int backstab_mult(int level);
int invalid_class(struct char_data *ch, struct obj_data *obj);
int level_exp(int chclass, int level);
const char *title_male(int chclass, int level);
const char *title_female(int chclass, int level);

/* Names first */

const char *class_abbrevs[] = {
  "Mu",
  "Cl",
  "Bh",
  "Wa",
  "Hr"
  "\n"
};


const char *pc_class_types[] = {
  "Magic User",
  "Cleric",
  "Bounty Hunter",
  "Warrior",
  "Hell Raiser"
  "\n"
};


/* The menu for choosing a class in interpreter.c: */
const char *class_menu =
"\r\n"
"Select a class:\r\n"
"  [C]leric\r\n"
"  [B]ounty-Hunter\r\n"
"  [W]arrior\r\n"
"  [M]agic-user\r\n"
"  [H]ell-raiser";



/*
 * The code to interpret a class letter -- used in interpreter.c when a
 * new character is selecting a class and by 'set class' in act.wizard.c.
 */

int parse_class(char arg)
{
  arg = LOWER(arg);

  switch (arg) {
  case 'm': return CLASS_MAGIC_USER;
  case 'c': return CLASS_CLERIC;
  case 'w': return CLASS_WARRIOR;
  case 'b': return CLASS_BOUNTY_HUNTER;
  case 'h': return CLASS_HELL_RAISER;
  default:  return CLASS_UNDEFINED;
  }
}

/*
 * bitvectors (i.e., powers of two) for each class, mainly for use in
 * do_who and do_users.  Add new classes at the end so that all classes
 * use sequential powers of two (1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4,
 * 1 << 5, etc.) up to the limit of your bitvector_t, typically 0-31.
 */
bitvector_t find_class_bitvector(const char *arg)
{
  size_t rpos, ret = 0;

  for (rpos = 0; rpos < strlen(arg); rpos++)
    ret |= (1 << parse_class(arg[rpos]));

  return (ret);
}


/*
 * These are definitions which control the guildmasters for each class.
 *
 * The first field (top line) controls the highest percentage skill level
 * a character of the class is allowed to attain in any skill.  (After
 * this level, attempts to practice will say "You are already learned in
 * this area."
 * 
 * The second line controls the maximum percent gain in learnedness a
 * character is allowed per practice -- in other words, if the random
 * die throw comes out higher than this number, the gain will only be
 * this number instead.
 *
 * The third line controls the minimu percent gain in learnedness a
 * character is allowed per practice -- in other words, if the random
 * die throw comes out below this number, the gain will be set up to
 * this number.
 * 
 * The fourth line simply sets whether the character knows 'spells'
 * or 'skills'.  This does not affect anything except the message given
 * to the character when trying to practice (i.e. "You know of the
 * following spells" vs. "You know of the following skills"
 */

#define SPELL	0
#define SKILL	1

/* #define LEARNED_LEVEL	0  % known which is considered "learned" */
/* #define MAX_PER_PRAC		1  max percent gain in skill per practice */
/* #define MIN_PER_PRAC		2  min percent gain in skill per practice */
/* #define PRAC_TYPE		3  should it say 'spell' or 'skill'?	*/

int prac_params[4][NUM_CLASSES] = {
  /* MAG	CLE	BTH	WAR HELL*/
  { 95,		95,  85, 80, 70	},	/* learned level */
  { 100,	100, 85, 12, 70	},	/* max per practice */
  { 25,		25,  0,  0,  70	},	/* min per practice */
  { SPELL,	SPELL,	SKILL,	SKILL	},	/* prac name */
};


/*
 * ...And the appropriate rooms for each guildmaster/guildguard; controls
 * which types of people the various guildguards let through.  i.e., the
 * first line shows that from room 18, only MAGIC_USERS are allowed
 * to go south.
 *
 * Don't forget to visit spec_assign.c if you create any new mobiles that
 * should be a guild master or guard so they can act appropriately. If you
 * "recycle" the existing mobs that are used in other guilds for your new
 * guild, then you don't have to change that file, only here.
 */
struct guild_info_type guild_info[] = {

/* Midgaard */
  { -999,	15,	SCMD_SOUTH	},
  { -999,	3004,	SCMD_NORTH	},
  { -999,	3027,	SCMD_EAST	},
  { -999,	3021,	SCMD_EAST	},

/* Brass Dragon */
  { -999 /* all */ ,	5065,	SCMD_WEST	},

/* this must go last -- add new guards above! */
  { -1, NOWHERE, -1}
};

/*
 * Saving throws for:
 * MCTW
 *   PARA, ROD, PETRI, BREATH, SPELL
 *     Levels 0-40
 *
 * Do not forget to change extern declaration in magic.c if you add to this.
 */

byte saving_throws(int class_num, int type, int level)
{
  /* Should not get here unless something is wrong. */
  return 100;
}

/* THAC0 for classes and levels.  (To Hit Armor Class 0) */
int thaco(struct char_data *ch) {
  struct obj_data *wielded = GET_EQ(ch, WEAR_WIELD);

  if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON && GET_OBJ_VAL(wielded, 0)) {
    switch(GET_OBJ_VAL(wielded, 0)){
      case(SKILL_MACHINE): // Machine gun
        return(HIT_MACHINE);
        break;
      case(SKILL_SHOTGUN): // Shotgun
        return(HIT_SHOTGUN);
        break;
      case(SKILL_PISTOL): // Pistol
        return(HIT_PISTOL);
        break;
      case(SKILL_RIFLE): // Rifle
        return(HIT_RIFLE);
        break;
      default:
        return(50);
        break;
   	 }
	}
    //return (GET_SKILL(ch, GET_OBJ_VAL(wielded, 0)));
   return 50;
}


/*
 * Roll the 6 stats for a character... each stat is made of the sum of
 * the best 3 out of 4 rolls of a 6-sided die.  Each class then decides
 * which priority will be given for the best to worst stats.
 */
void roll_real_abils(struct char_data *ch)
{
  int i, j, k, temp;
  ubyte table[6];
  ubyte rolls[4];

  for (i = 0; i < 6; i++)
    table[i] = 0;

  for (i = 0; i < 6; i++) {

    for (j = 0; j < 4; j++)
      rolls[j] = rand_number(1, 6);

    temp = rolls[0] + rolls[1] + rolls[2] + rolls[3] -
      MIN(rolls[0], MIN(rolls[1], MIN(rolls[2], rolls[3])));

    for (k = 0; k < 6; k++)
      if (table[k] < temp) {
	temp ^= table[k];
	table[k] ^= temp;
	temp ^= table[k];
      }
  }

  ch->real_abils.str_add = 0;

  switch (GET_CLASS(ch)) {
  case CLASS_MAGIC_USER:
    ch->real_abils.intel = table[0];
    ch->real_abils.wis = table[1];
    ch->real_abils.dex = table[2];
    ch->real_abils.str = table[3];
    ch->real_abils.con = table[4];
    ch->real_abils.cha = table[5];
    break;
  case CLASS_CLERIC:
    ch->real_abils.wis = table[0];
    ch->real_abils.intel = table[1];
    ch->real_abils.str = table[2];
    ch->real_abils.dex = table[3];
    ch->real_abils.con = table[4];
    ch->real_abils.cha = table[5];
    break;
  case CLASS_BOUNTY_HUNTER:
    ch->real_abils.dex = table[0];
    ch->real_abils.str = table[1];
    ch->real_abils.con = table[2];
    ch->real_abils.intel = table[3];
    ch->real_abils.wis = table[4];
    ch->real_abils.cha = table[5];
    break;
  case CLASS_WARRIOR:
    ch->real_abils.str = table[0];
    ch->real_abils.dex = table[1];
    ch->real_abils.con = table[2];
    ch->real_abils.wis = table[3];
    ch->real_abils.intel = table[4];
    ch->real_abils.cha = table[5];
    if (ch->real_abils.str == 18)
      ch->real_abils.str_add = rand_number(0, 100);
    break;
  case CLASS_HELL_RAISER:
    ch->real_abils.con = table[0];
    ch->real_abils.dex = table[1];
    ch->real_abils.str = table[2];
    ch->real_abils.intel = table[3];
    ch->real_abils.wis = table[4];
    ch->real_abils.cha = table[5];
    break;
  }
  ch->aff_abils = ch->real_abils;
}


/* Some initializations for characters, including initial skills */
void do_start(struct char_data *ch)
{
  GET_LEVEL(ch) = 1;
  GET_EXP(ch) = 1;
  GET_ATTACKS(ch) = 1;

  set_title(ch, NULL);
  roll_real_abils(ch);

  GET_MAX_HIT(ch)  = 100;
  GET_MAX_MANA(ch) = 100;

  switch (GET_CLASS(ch)) {

  case CLASS_MAGIC_USER:
    break;

  case CLASS_CLERIC:
    break;

  case CLASS_BOUNTY_HUNTER:
    GET_EVASION(ch) = 5;
    SET_SKILL(ch, SKILL_SNEAK, 10);
    SET_SKILL(ch, SKILL_HIDE, 5);
    SET_SKILL(ch, SKILL_STEAL, 15);
    SET_SKILL(ch, SKILL_BACKSTAB, 10);
    SET_SKILL(ch, SKILL_PICK_LOCK, 10);
    SET_SKILL(ch, SKILL_TRACK, 10);
    SET_SKILL(ch, SKILL_RIFLE, 1);
    SET_SKILL(ch, SKILL_PISTOL, 1);
    break;

  case CLASS_WARRIOR:
    break;

  case CLASS_HELL_RAISER:
    GET_EVASION(ch) = 0;
    SET_SKILL(ch, SKILL_MACHINE, 1);
    SET_SKILL(ch, SKILL_SHOTGUN, 1);
    break;
  }

  advance_level(ch);
  mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s advanced to level %d", GET_NAME(ch), GET_LEVEL(ch));

  GET_HIT(ch) = GET_MAX_HIT(ch);
  GET_MANA(ch) = GET_MAX_MANA(ch);

  GET_COND(ch, DRUNK) = 0;

  if (CONFIG_SITEOK_ALL)
    SET_BIT(PLR_FLAGS(ch), PLR_SITEOK);

  ch->player_specials->saved.olc_zone = NOWHERE;
}



/*
 * This function controls the change to maxmana and maxhp for
 * each class every time they gain a level.
 */
void advance_level(struct char_data *ch)
{
  int add_hp, add_mana = 0, i;

  if (GET_SKILL(ch, SKILL_TOUGHNESS)) // Give bonus hp
    add_hp = TOUGHNESS_BONUS_HP;

  add_hp = con_app[GET_CON(ch)].hitp;

  switch (GET_CLASS(ch)) {

  case CLASS_MAGIC_USER:
    add_hp += rand_number(3, 8);
    add_mana = rand_number(GET_LEVEL(ch), (int)(1.5 * GET_LEVEL(ch)));
    add_mana = MIN(add_mana, 10);
    break;

  case CLASS_CLERIC:
    add_hp += rand_number(5, 10);
    add_mana = rand_number(GET_LEVEL(ch), (int)(1.5 * GET_LEVEL(ch)));
    add_mana = MIN(add_mana, 10);
    break;

  case CLASS_BOUNTY_HUNTER:
    add_hp += rand_number(7, 13);
    add_mana = 0;
    break;

  case CLASS_WARRIOR:
    add_hp += rand_number(10, 15);
    add_mana = 0;
    break;
  default:
    add_hp += rand_number(5, 10);
  }

  ch->points.max_hit += MAX(1, add_hp);

  if (GET_LEVEL(ch) > 1)
    ch->points.max_mana += add_mana;

  if (IS_MAGIC_USER(ch) || IS_CLERIC(ch))
    GET_PRACTICES(ch) += 1; 
  else
    GET_PRACTICES(ch) += 1; 

  if (GET_LEVEL(ch) >= LVL_IMMORT) {
    for (i = 0; i < 3; i++)
      GET_COND(ch, i) = (char) -1;
    SET_BIT(PRF_FLAGS(ch), PRF_HOLYLIGHT);
  }

  snoop_check(ch);
  save_char(ch);
}


/*
 * This simply calculates the backstab multiplier based on a character's
 * level.  This used to be an array, but was changed to be a function so
 * that it would be easier to add more levels to your MUD.  This doesn't
 * really create a big performance hit because it's not used very often.
 */
int backstab_mult(int level)
{
  if (level <= 0)
    return 1;	  /* level 0 */
  else if (level <= 7)
    return 2;	  /* level 1 - 7 */
  else if (level <= 13)
    return 3;	  /* level 8 - 13 */
  else if (level <= 20)
    return 4;	  /* level 14 - 20 */
  else if (level <= 28)
    return 5;	  /* level 21 - 28 */
  else if (level < LVL_IMMORT)
    return 6;	  /* all remaining mortal levels */
  else
    return 20;	  /* immortals */
}
/* Returns the percentage to get a headshot. */
int headshot_percentage(int level)
{
  if (level <= 0)
    return 0;   /* level 0 */
  else if (level <= 7)
    return 65;   /* level 1 - 7 */
  else if (level <= 13)
    return 70;   /* level 8 - 13 */
  else if (level <= 20)
    return 75;   /* level 14 - 20 */
  else if (level <= 28)
    return 80;   /* level 21 - 28 */
  else if (level < LVL_IMMORT)
    return 85;   /* all remaining mortal levels */
  else
    return 100;    /* immortals */
}
int headshot_damage(int level)
{
  if (level <= 0)
    return 0;         /* level 0 */
  else if (level <= 7)
    return 3;         /* level 1 - 7 */
  else if (level <= 13)
    return 4;         /* level 8 - 13 */
  else if (level <= 20)
    return 5;         /* level 14 - 20 */
  else if (level <= 28)
    return 6;         /* level 21 - 28 */
  else if (level < LVL_IMMORT)
    return 7;         /* all remaining mortal levels */
  else
    return 100;       /* immortals */
}

int masochism_damage(ch){
  int missing_health = GET_MAX_HIT(ch) - GET_HIT(ch);
  return  (missing_health - 10)/15;
}

/*
 * invalid_class is used by handler.c to determine if a piece of equipment is
 * usable by a particular class, based on the ITEM_ANTI_{class} bitvectors.
 */
int invalid_class(struct char_data *ch, struct obj_data *obj)
{
  if (OBJ_FLAGGED(obj, ITEM_ANTI_MAGIC_USER) && IS_MAGIC_USER(ch))
    return TRUE;

  if (OBJ_FLAGGED(obj, ITEM_ANTI_CLERIC) && IS_CLERIC(ch))
    return TRUE;

  if (OBJ_FLAGGED(obj, ITEM_ANTI_WARRIOR) && IS_WARRIOR(ch))
    return TRUE;

  if (OBJ_FLAGGED(obj, ITEM_ANTI_THIEF) && IS_THIEF(ch))
    return TRUE;

  return FALSE;
}


/*
 * SPELLS AND SKILLS.  This area defines which spells are assigned to
 * which classes, and the minimum level the character must be to use
 * the spell or skill.
 */
void init_spell_levels(void)
{
  /* MAGES */
  spell_level(SPELL_MAGIC_MISSILE, CLASS_MAGIC_USER, 1);
  spell_level(SPELL_DETECT_INVIS, CLASS_MAGIC_USER, 2);
  spell_level(SPELL_DETECT_MAGIC, CLASS_MAGIC_USER, 2);
  spell_level(SPELL_CHILL_TOUCH, CLASS_MAGIC_USER, 3);
  spell_level(SPELL_INFRAVISION, CLASS_MAGIC_USER, 3);
  spell_level(SPELL_INVISIBLE, CLASS_MAGIC_USER, 4);
  spell_level(SPELL_ARMOR, CLASS_MAGIC_USER, 4);
  spell_level(SPELL_BURNING_HANDS, CLASS_MAGIC_USER, 5);
  spell_level(SPELL_LOCATE_OBJECT, CLASS_MAGIC_USER, 6);
  spell_level(SPELL_STRENGTH, CLASS_MAGIC_USER, 6);
  spell_level(SPELL_SHOCKING_GRASP, CLASS_MAGIC_USER, 7);
  spell_level(SPELL_SLEEP, CLASS_MAGIC_USER, 8);
  spell_level(SPELL_LIGHTNING_BOLT, CLASS_MAGIC_USER, 9);
  spell_level(SPELL_BLINDNESS, CLASS_MAGIC_USER, 9);
  spell_level(SPELL_DETECT_POISON, CLASS_MAGIC_USER, 10);
  spell_level(SPELL_COLOR_SPRAY, CLASS_MAGIC_USER, 11);
  spell_level(SPELL_ENERGY_DRAIN, CLASS_MAGIC_USER, 13);
  spell_level(SPELL_CURSE, CLASS_MAGIC_USER, 14);
  spell_level(SPELL_POISON, CLASS_MAGIC_USER, 14);
  spell_level(SPELL_FIREBALL, CLASS_MAGIC_USER, 15);
  spell_level(SPELL_CHARM, CLASS_MAGIC_USER, 16);
  spell_level(SPELL_ENCHANT_WEAPON, CLASS_MAGIC_USER, 26);
  spell_level(SPELL_CLONE, CLASS_MAGIC_USER, 30);

  /* CLERICS */
  spell_level(SPELL_CURE_LIGHT, CLASS_CLERIC, 1);
  spell_level(SPELL_ARMOR, CLASS_CLERIC, 1);
  spell_level(SPELL_CREATE_FOOD, CLASS_CLERIC, 2);
  spell_level(SPELL_CREATE_WATER, CLASS_CLERIC, 2);
  spell_level(SPELL_DETECT_POISON, CLASS_CLERIC, 3);
  spell_level(SPELL_DETECT_ALIGN, CLASS_CLERIC, 4);
  spell_level(SPELL_CURE_BLIND, CLASS_CLERIC, 4);
  spell_level(SPELL_BLESS, CLASS_CLERIC, 5);
  spell_level(SPELL_DETECT_INVIS, CLASS_CLERIC, 6);
  spell_level(SPELL_BLINDNESS, CLASS_CLERIC, 6);
  spell_level(SPELL_INFRAVISION, CLASS_CLERIC, 7);
  spell_level(SPELL_PROT_FROM_EVIL, CLASS_CLERIC, 8);
  spell_level(SPELL_POISON, CLASS_CLERIC, 8);
  spell_level(SPELL_GROUP_ARMOR, CLASS_CLERIC, 9);
  spell_level(SPELL_CURE_CRITIC, CLASS_CLERIC, 9);
  spell_level(SPELL_SUMMON, CLASS_CLERIC, 10);
  spell_level(SPELL_REMOVE_POISON, CLASS_CLERIC, 10);
  spell_level(SPELL_WORD_OF_RECALL, CLASS_CLERIC, 12);
  spell_level(SPELL_EARTHQUAKE, CLASS_CLERIC, 12);
  spell_level(SPELL_DISPEL_EVIL, CLASS_CLERIC, 14);
  spell_level(SPELL_DISPEL_GOOD, CLASS_CLERIC, 14);
  spell_level(SPELL_SANCTUARY, CLASS_CLERIC, 15);
  spell_level(SPELL_CALL_LIGHTNING, CLASS_CLERIC, 15);
  spell_level(SPELL_HEAL, CLASS_CLERIC, 16);
  spell_level(SPELL_CONTROL_WEATHER, CLASS_CLERIC, 17);
  spell_level(SPELL_SENSE_LIFE, CLASS_CLERIC, 18);
  spell_level(SPELL_HARM, CLASS_CLERIC, 19);
  spell_level(SPELL_GROUP_HEAL, CLASS_CLERIC, 22);
  spell_level(SPELL_REMOVE_CURSE, CLASS_CLERIC, 26);

  /* BOUNTY HUNTERS */
  spell_level(SKILL_SNEAK, CLASS_BOUNTY_HUNTER, 1);
  spell_level(SKILL_PICK_LOCK, CLASS_BOUNTY_HUNTER, 2);
  spell_level(SKILL_BACKSTAB, CLASS_BOUNTY_HUNTER, 3);
  spell_level(SKILL_STEAL, CLASS_BOUNTY_HUNTER, 4);
  spell_level(SKILL_HIDE, CLASS_BOUNTY_HUNTER, 5);
  spell_level(SKILL_TRACK, CLASS_BOUNTY_HUNTER, 6);
  spell_level(SKILL_PISTOL, CLASS_BOUNTY_HUNTER, 1);
  spell_level(SKILL_RIFLE, CLASS_BOUNTY_HUNTER, 1);

  /* WARRIORS */
  spell_level(SKILL_KICK, CLASS_WARRIOR, 1);
  spell_level(SKILL_RESCUE, CLASS_WARRIOR, 3);
  spell_level(SKILL_TRACK, CLASS_WARRIOR, 9);
  spell_level(SKILL_BASH, CLASS_WARRIOR, 12);

  /* HELL RAISERS */
  spell_level(SKILL_MACHINE, CLASS_HELL_RAISER, 1);
  spell_level(SKILL_SHOTGUN, CLASS_HELL_RAISER,  1);  
  spell_level(SKILL_PISTOL, CLASS_HELL_RAISER, 1);
  spell_level(SKILL_RIFLE, CLASS_HELL_RAISER, 1);

  spell_level(SKILL_HEAL_PACK, CLASS_HELL_RAISER, 1);
  spell_level(SKILL_EVASION, CLASS_HELL_RAISER,  1);  
  spell_level(SKILL_DUAL_PISTOL, CLASS_HELL_RAISER, 1);   //Need secondary weapon first.
  spell_level(SKILL_QUICK_RELOAD, CLASS_HELL_RAISER, 1); // Need reload to work first

  spell_level(SKILL_MASOCHISM, CLASS_HELL_RAISER, 1);
  spell_level(SKILL_TOUGHNESS, CLASS_HELL_RAISER,  1);  
  spell_level(SKILL_ACCURACY, CLASS_HELL_RAISER, 1);
  spell_level(SKILL_CRITICAL, CLASS_HELL_RAISER, 1);

  spell_level(SKILL_ADRENALINE, CLASS_HELL_RAISER, 1);
  spell_level(SKILL_COUNTER_ATTACK, CLASS_HELL_RAISER,  1);  
  spell_level(SKILL_BLEED_CRIT, CLASS_HELL_RAISER, 1);
  spell_level(SKILL_HEAL_CRIT, CLASS_HELL_RAISER, 1);

  spell_level(SKILL_RAPID_FIRE, CLASS_HELL_RAISER, 1);    // Need attack speed working
  spell_level(SKILL_BERSERK, CLASS_HELL_RAISER,  1);  
  spell_level(SKILL_PIERCE_SHOT, CLASS_HELL_RAISER, 1); // Need armor working first
  spell_level(SKILL_TURRET, CLASS_HELL_RAISER, 1);      // Turret as a pet?

  spell_level(SKILL_HEADSHOT, CLASS_HELL_RAISER, 1);
  spell_level(SKILL_EXTEND_MAG, CLASS_HELL_RAISER,  1);  
  spell_level(SKILL_PIERCE_SHOT, CLASS_HELL_RAISER, 1);
  spell_level(SKILL_SLOW_SHOT, CLASS_HELL_RAISER, 1);   // Need attack speed working
}


/*
 * This is the exp given to implementors -- it must always be greater
 * than the exp required for immortality, plus at least 20,000 or so.
 */
#define EXP_MAX  10000000

/* Function to return the exp required for each class/level */
int level_exp(int chclass, int level)
{
  if (level > LVL_IMPL || level < 0) {
    log("SYSERR: Requesting exp for invalid level %d!", level);
    return 0;
  }

  /*
   * Gods have exp close to EXP_MAX.  This statement should never have to
   * changed, regardless of how many mortal or immortal levels exist.
   */
   if (level > LVL_IMMORT) {
     return EXP_MAX - ((LVL_IMPL - level) * 1000);
   }

  /* Exp required for normal mortals is below */

  switch (chclass) {

    case CLASS_MAGIC_USER:
    switch (level) {
      case  0: return 0;
      case  1: return 1;
      case  2: return 2500;
      case  3: return 5000;
      case  4: return 10000;
      case  5: return 20000;
      case  6: return 40000;
      case  7: return 60000;
      case  8: return 90000;
      case  9: return 135000;
      case 10: return 250000;
      case 11: return 375000;
      case 12: return 750000;
      case 13: return 1125000;
      case 14: return 1500000;
      case 15: return 1875000;
      case 16: return 2250000;
      case 17: return 2625000;
      case 18: return 3000000;
      case 19: return 3375000;
      case 20: return 3750000;
      case 21: return 4000000;
      case 22: return 4300000;
      case 23: return 4600000;
      case 24: return 4900000;
      case 25: return 5200000;
      case 26: return 5500000;
      case 27: return 5950000;
      case 28: return 6400000;
      case 29: return 6850000;
      case 30: return 7400000;
      /* add new levels here */
      case LVL_IMMORT: return 8000000;
    }
    break;

    case CLASS_CLERIC:
    switch (level) {
      case  0: return 0;
      case  1: return 1;
      case  2: return 1500;
      case  3: return 3000;
      case  4: return 6000;
      case  5: return 13000;
      case  6: return 27500;
      case  7: return 55000;
      case  8: return 110000;
      case  9: return 225000;
      case 10: return 450000;
      case 11: return 675000;
      case 12: return 900000;
      case 13: return 1125000;
      case 14: return 1350000;
      case 15: return 1575000;
      case 16: return 1800000;
      case 17: return 2100000;
      case 18: return 2400000;
      case 19: return 2700000;
      case 20: return 3000000;
      case 21: return 3250000;
      case 22: return 3500000;
      case 23: return 3800000;
      case 24: return 4100000;
      case 25: return 4400000;
      case 26: return 4800000;
      case 27: return 5200000;
      case 28: return 5600000;
      case 29: return 6000000;
      case 30: return 6400000;
      /* add new levels here */
      case LVL_IMMORT: return 7000000;
    }
    break;

    case CLASS_BOUNTY_HUNTER:
    switch (level) {
      case  0: return 0;
      case  1: return 1;
      case  2: return 1250;
      case  3: return 2500;
      case  4: return 5000;
      case  5: return 10000;
      case  6: return 20000;
      case  7: return 40000;
      case  8: return 70000;
      case  9: return 110000;
      case 10: return 160000;
      case 11: return 220000;
      case 12: return 440000;
      case 13: return 660000;
      case 14: return 880000;
      case 15: return 1100000;
      case 16: return 1500000;
      case 17: return 2000000;
      case 18: return 2500000;
      case 19: return 3000000;
      case 20: return 3500000;
      case 21: return 3650000;
      case 22: return 3800000;
      case 23: return 4100000;
      case 24: return 4400000;
      case 25: return 4700000;
      case 26: return 5100000;
      case 27: return 5500000;
      case 28: return 5900000;
      case 29: return 6300000;
      case 30: return 6650000;
      /* add new levels here */
      case LVL_IMMORT: return 7000000;
    }
    break;

    case CLASS_WARRIOR:
    switch (level) {
      case  0: return 0;
      case  1: return 1;
      case  2: return 2000;
      case  3: return 4000;
      case  4: return 8000;
      case  5: return 16000;
      case  6: return 32000;
      case  7: return 64000;
      case  8: return 125000;
      case  9: return 250000;
      case 10: return 500000;
      case 11: return 750000;
      case 12: return 1000000;
      case 13: return 1250000;
      case 14: return 1500000;
      case 15: return 1850000;
      case 16: return 2200000;
      case 17: return 2550000;
      case 18: return 2900000;
      case 19: return 3250000;
      case 20: return 3600000;
      case 21: return 3900000;
      case 22: return 4200000;
      case 23: return 4500000;
      case 24: return 4800000;
      case 25: return 5150000;
      case 26: return 5500000;
      case 27: return 5950000;
      case 28: return 6400000;
      case 29: return 6850000;
      case 30: return 7400000;
      /* add new levels here */
      case LVL_IMMORT: return 8000000;
    }
    break;
  }

  /*
   * This statement should never be reached if the exp tables in this function
   * are set up properly.  If you see exp of 123456 then the tables above are
   * incomplete -- so, complete them!
   */
  log("SYSERR: XP tables not set up correctly in class.c!");
  return 123456;
}


/* 
 * Default titles of male characters.
 */
const char *title_male(int chclass, int level)
{
  if (level <= 0 || level > LVL_IMPL)
    return "the Man";
  if (level == LVL_IMPL)
    return "the Implementor";

  switch (chclass) {

    case CLASS_MAGIC_USER:
    switch (level) {
      case  1: return "the Apprentice of Magic";
      case  2: return "the Spell Student";
      case  3: return "the Scholar of Magic";
      case  4: return "the Delver in Spells";
      case  5: return "the Medium of Magic";
      case  6: return "the Scribe of Magic";
      case  7: return "the Seer";
      case  8: return "the Sage";
      case  9: return "the Illusionist";
      case 10: return "the Abjurer";
      case 11: return "the Invoker";
      case 12: return "the Enchanter";
      case 13: return "the Conjurer";
      case 14: return "the Magician";
      case 15: return "the Creator";
      case 16: return "the Savant";
      case 17: return "the Magus";
      case 18: return "the Wizard";
      case 19: return "the Warlock";
      case 20: return "the Sorcerer";
      case 21: return "the Necromancer";
      case 22: return "the Thaumaturge";
      case 23: return "the Student of the Occult";
      case 24: return "the Disciple of the Uncanny";
      case 25: return "the Minor Elemental";
      case 26: return "the Greater Elemental";
      case 27: return "the Crafter of Magics";
      case 28: return "the Shaman";
      case 29: return "the Keeper of Talismans";
      case 30: return "the Archmage";
      case LVL_IMMORT: return "the Immortal Warlock";
      case LVL_GOD: return "the Avatar of Magic";
      case LVL_GRGOD: return "the God of Magic";
      default: return "the Mage";
    }
    break;

    case CLASS_CLERIC:
    switch (level) {
      case  1: return "the Believer";
      case  2: return "the Attendant";
      case  3: return "the Acolyte";
      case  4: return "the Novice";
      case  5: return "the Missionary";
      case  6: return "the Adept";
      case  7: return "the Deacon";
      case  8: return "the Vicar";
      case  9: return "the Priest";
      case 10: return "the Minister";
      case 11: return "the Canon";
      case 12: return "the Levite";
      case 13: return "the Curate";
      case 14: return "the Monk";
      case 15: return "the Healer";
      case 16: return "the Chaplain";
      case 17: return "the Expositor";
      case 18: return "the Bishop";
      case 19: return "the Arch Bishop";
      case 20: return "the Patriarch";
      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Cardinal";
      case LVL_GOD: return "the Inquisitor";
      case LVL_GRGOD: return "the God of good and evil";
      default: return "the Cleric";
    }
    break;

    case CLASS_BOUNTY_HUNTER:
    switch (level) {
      case  1: return "the Pilferer";
      case  2: return "the Footpad";
      case  3: return "the Filcher";
      case  4: return "the Pick-Pocket";
      case  5: return "the Sneak";
      case  6: return "the Pincher";
      case  7: return "the Cut-Purse";
      case  8: return "the Snatcher";
      case  9: return "the Sharper";
      case 10: return "the Rogue";
      case 11: return "the Robber";
      case 12: return "the Magsman";
      case 13: return "the Highwayman";
      case 14: return "the Burglar";
      case 15: return "the Thief";
      case 16: return "the Knifer";
      case 17: return "the Quick-Blade";
      case 18: return "the Killer";
      case 19: return "the Brigand";
      case 20: return "the Cut-Throat";
      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Assassin";
      case LVL_GOD: return "the Demi God of thieves";
      case LVL_GRGOD: return "the God of thieves and tradesmen";
      default: return "the Thief";
    }
    break;

    case CLASS_WARRIOR:
    switch(level) {
      case  1: return "the Swordpupil";
      case  2: return "the Recruit";
      case  3: return "the Sentry";
      case  4: return "the Fighter";
      case  5: return "the Soldier";
      case  6: return "the Warrior";
      case  7: return "the Veteran";
      case  8: return "the Swordsman";
      case  9: return "the Fencer";
      case 10: return "the Combatant";
      case 11: return "the Hero";
      case 12: return "the Myrmidon";
      case 13: return "the Swashbuckler";
      case 14: return "the Mercenary";
      case 15: return "the Swordmaster";
      case 16: return "the Lieutenant";
      case 17: return "the Champion";
      case 18: return "the Dragoon";
      case 19: return "the Cavalier";
      case 20: return "the Knight";
      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Warlord";
      case LVL_GOD: return "the Extirpator";
      case LVL_GRGOD: return "the God of war";
      default: return "the Warrior";
    }
    break;
  }

  /* Default title for classes which do not have titles defined */
  return "the Classless";
}


/* 
 * Default titles of female characters.
 */
const char *title_female(int chclass, int level)
{
  if (level <= 0 || level > LVL_IMPL)
    return "the Woman";
  if (level == LVL_IMPL)
    return "the Implementress";

  switch (chclass) {

    case CLASS_MAGIC_USER:
    switch (level) {
      case  1: return "the Apprentice of Magic";
      case  2: return "the Spell Student";
      case  3: return "the Scholar of Magic";
      case  4: return "the Delveress in Spells";
      case  5: return "the Medium of Magic";
      case  6: return "the Scribess of Magic";
      case  7: return "the Seeress";
      case  8: return "the Sage";
      case  9: return "the Illusionist";
      case 10: return "the Abjuress";
      case 11: return "the Invoker";
      case 12: return "the Enchantress";
      case 13: return "the Conjuress";
      case 14: return "the Witch";
      case 15: return "the Creator";
      case 16: return "the Savant";
      case 17: return "the Craftess";
      case 18: return "the Wizard";
      case 19: return "the War Witch";
      case 20: return "the Sorceress";
      case 21: return "the Necromancress";
      case 22: return "the Thaumaturgess";
      case 23: return "the Student of the Occult";
      case 24: return "the Disciple of the Uncanny";
      case 25: return "the Minor Elementress";
      case 26: return "the Greater Elementress";
      case 27: return "the Crafter of Magics";
      case 28: return "Shaman";
      case 29: return "the Keeper of Talismans";
      case 30: return "Archwitch";
      case LVL_IMMORT: return "the Immortal Enchantress";
      case LVL_GOD: return "the Empress of Magic";
      case LVL_GRGOD: return "the Goddess of Magic";
      default: return "the Witch";
    }
    break;

    case CLASS_CLERIC:
    switch (level) {
      case  1: return "the Believer";
      case  2: return "the Attendant";
      case  3: return "the Acolyte";
      case  4: return "the Novice";
      case  5: return "the Missionary";
      case  6: return "the Adept";
      case  7: return "the Deaconess";
      case  8: return "the Vicaress";
      case  9: return "the Priestess";
      case 10: return "the Lady Minister";
      case 11: return "the Canon";
      case 12: return "the Levitess";
      case 13: return "the Curess";
      case 14: return "the Nunne";
      case 15: return "the Healess";
      case 16: return "the Chaplain";
      case 17: return "the Expositress";
      case 18: return "the Bishop";
      case 19: return "the Arch Lady of the Church";
      case 20: return "the Matriarch";
      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Priestess";
      case LVL_GOD: return "the Inquisitress";
      case LVL_GRGOD: return "the Goddess of good and evil";
      default: return "the Cleric";
    }
    break;

    case CLASS_BOUNTY_HUNTER:
    switch (level) {
      case  1: return "the Pilferess";
      case  2: return "the Footpad";
      case  3: return "the Filcheress";
      case  4: return "the Pick-Pocket";
      case  5: return "the Sneak";
      case  6: return "the Pincheress";
      case  7: return "the Cut-Purse";
      case  8: return "the Snatcheress";
      case  9: return "the Sharpress";
      case 10: return "the Rogue";
      case 11: return "the Robber";
      case 12: return "the Magswoman";
      case 13: return "the Highwaywoman";
      case 14: return "the Burglaress";
      case 15: return "the Thief";
      case 16: return "the Knifer";
      case 17: return "the Quick-Blade";
      case 18: return "the Murderess";
      case 19: return "the Brigand";
      case 20: return "the Cut-Throat";
      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Assassin";
      case LVL_GOD: return "the Demi Goddess of thieves";
      case LVL_GRGOD: return "the Goddess of thieves and tradesmen";
      default: return "the Thief";
    }
    break;

    case CLASS_WARRIOR:
    switch(level) {
      case  1: return "the Swordpupil";
      case  2: return "the Recruit";
      case  3: return "the Sentress";
      case  4: return "the Fighter";
      case  5: return "the Soldier";
      case  6: return "the Warrior";
      case  7: return "the Veteran";
      case  8: return "the Swordswoman";
      case  9: return "the Fenceress";
      case 10: return "the Combatess";
      case 11: return "the Heroine";
      case 12: return "the Myrmidon";
      case 13: return "the Swashbuckleress";
      case 14: return "the Mercenaress";
      case 15: return "the Swordmistress";
      case 16: return "the Lieutenant";
      case 17: return "the Lady Champion";
      case 18: return "the Lady Dragoon";
      case 19: return "the Cavalier";
      case 20: return "the Lady Knight";
      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Lady of War";
      case LVL_GOD: return "the Queen of Destruction";
      case LVL_GRGOD: return "the Goddess of war";
      default: return "the Warrior";
    }
    break;
  }

  /* Default title for classes which do not have titles defined */
  return "the Classless";
}

