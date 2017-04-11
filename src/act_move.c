/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  Envy Diku Mud improvements copyright (C) 1994 by Michael Quan, David   *
 *  Love, Guilherme 'Willie' Arnold, and Mitchell Tse.                     *
 *                                                                         *
 *  EnvyMud 2.0 improvements copyright (C) 1995 by Michael Quan and        *
 *  Mitchell Tse.                                                          *
 *                                                                         *
 *  EnvyMud 2.2 improvements copyright (C) 1996, 1997 by Michael Quan.     *
 *                                                                         *
 *  In order to use any part of this Envy Diku Mud, you must comply with   *
 *  the original Diku license in 'license.doc', the Merc license in        *
 *  'license.txt', as well as the Envy license in 'license.nvy'.           *
 *  In particular, you may not remove either of these copyright notices.   *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

#if defined( macintosh )
#include <types.h>
#else
#include <sys/types.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merc.h"



char *	const	dir_name	[ ]		=
{
    "north", "east", "south", "west", "up", "down"
};

const	int	rev_dir		[ ]		=
{
    2, 3, 0, 1, 5, 4
};

const	int	movement_loss	[ SECT_MAX ]	=
{
    1, 2, 2, 3, 4, 6, 4, 1, 5, 10, 6
};



/*
 * Local functions.
 */
int	find_door	args( ( CHAR_DATA *ch, char *arg ) );
bool	has_key		args( ( CHAR_DATA *ch, int key ) );

/*
 *  Local game functions.
 */
DECLARE_GAME_FUN( game_u_l_t );


void move_char( CHAR_DATA *ch, int door )
{
    CHAR_DATA       *fch;
    CHAR_DATA       *fch_next;
    EXIT_DATA       *pexit;
    ROOM_INDEX_DATA *in_room;
    ROOM_INDEX_DATA *to_room;
    int              moved = 131072; /* Matches ACT & PLR bits */

    if ( door < 0 || door > 5 )
    {
	bug( "Do_move: bad door %d.", door );
	return;
    }

    /*
     * Prevents infinite move loop in
     * maze zone when group has 2 leaders - Kahn
     */
    if ( IS_SET( ch->act, moved ) )
        return;

    if ( IS_AFFECTED( ch, AFF_HOLD ) ) 
    {
	send_to_char( "You are stuck in a snare!  You can't move!\r\n", ch );
	return;
    }

    in_room = ch->in_room;
    if ( !( pexit = in_room->exit[door] ) || !( to_room = pexit->to_room ) )
    {
	send_to_char( "Alas, you cannot go that way.\r\n", ch );
	return;
    }

    if ( IS_SET( pexit->exit_info, EX_CLOSED ) )
    {
        if ( !IS_AFFECTED( ch, AFF_PASS_DOOR )
	    && !IS_SET( race_table[ ch->race ].race_abilities,
		       RACE_PASSDOOR ) )
        {
	    act( "The $d is closed.",
		ch, NULL, pexit->keyword, TO_CHAR );
	    return;
	}
	if ( IS_SET( pexit->exit_info, EX_PASSPROOF ) )
        {
	    act( "You are unable to pass through the $d.  Ouch!",
		ch, NULL, pexit->keyword, TO_CHAR );
	    return;
	}
    }

    if ( IS_AFFECTED( ch, AFF_CHARM )
	&& ch->master
	&& in_room == ch->master->in_room )
    {
	send_to_char( "What?  And leave your beloved master?\r\n", ch );
	return;
    }

    if ( room_is_private( to_room ) )
    {
	send_to_char( "That room is private right now.\r\n", ch );
	return;
    }

    if ( !IS_NPC( ch ) )
    {
	int iClass;
	int move;

	for ( iClass = 0; iClass < MAX_CLASS; iClass++ )
	{
	    if ( iClass != ch->class
		&& to_room->vnum == class_table[iClass].guild )
	    {
		send_to_char( "You aren't allowed in there.\r\n", ch );
		return;
	    }
	}

	if (   in_room->sector_type == SECT_AIR
	    || to_room->sector_type == SECT_AIR )
	{
	    if ( !IS_AFFECTED( ch, AFF_FLYING )
		&& !IS_SET( race_table[ ch->race ].race_abilities, RACE_FLY ) )
	    {
		send_to_char( "You can't fly.\r\n", ch );
		return;
	    }
	}

	if (   to_room->sector_type != SECT_WATER_NOSWIM
	    && to_room->sector_type != SECT_UNDERWATER
	    && strcmp( race_table[ ch->race ].name, "God" )
	    && strcmp( race_table[ ch->race ].name, "Bear" )
	    && IS_SET( race_table[ ch->race ].race_abilities, RACE_SWIM ) )
	{
	    send_to_char( "You flap around but you cant move!\r\n", ch );
	    return;
	}


	if (   in_room->sector_type == SECT_WATER_NOSWIM
	    || to_room->sector_type == SECT_WATER_NOSWIM )
	{
	    OBJ_DATA *obj;
	    bool      found;

	    /*
	     * Look for a boat.
	     */
	    found = FALSE;

	    /*
	     * Suggestion for flying above water by Sludge
	     */
	    if ( IS_AFFECTED( ch, AFF_FLYING )
		|| IS_SET( race_table[ ch->race ].race_abilities,  RACE_FLY )
		|| IS_SET( race_table[ ch->race ].race_abilities,
			                                     RACE_WATERWALK )
		|| IS_SET( race_table[ ch->race ].race_abilities, RACE_SWIM ) )
	        found = TRUE;

	    for ( obj = ch->carrying; obj; obj = obj->next_content )
	    {
		if ( obj->item_type == ITEM_BOAT )
		{
		    found = TRUE;
		    break;
		}
	    }
	    if ( !found )
	    {
		send_to_char( "You need a boat to go there.\r\n", ch );
		return;
	    }
	}

	if ( (   in_room->sector_type == SECT_UNDERWATER
	      || to_room->sector_type == SECT_UNDERWATER )
	    &&   !IS_SET( race_table[ ch->race ].race_abilities, RACE_SWIM ) )
	{
	    send_to_char( "You need to be able to swim to go there.\r\n", ch );
	    return;
	}

	move = movement_loss[UMIN( SECT_MAX-1, in_room->sector_type )]
	     + movement_loss[UMIN( SECT_MAX-1, to_room->sector_type )]
	     ;
	/* Flying persons lose constant minimum movement. */
	if (   IS_SET( race_table[ ch->race ].race_abilities, RACE_FLY )
	    || IS_AFFECTED( ch, AFF_FLYING ) )
	    move = 2;

	if ( ch->move < move )
	{
	    send_to_char( "You are too exhausted.\r\n", ch );
	    return;
	}

	WAIT_STATE( ch, 1 );
	ch->move -= move;
    }

    if ( !IS_AFFECTED( ch, AFF_SNEAK ) && ( IS_NPC( ch ) || !IS_SET( ch->act, PLR_WIZINVIS ) ) )
    {
        if (     (   ( in_room->sector_type == SECT_WATER_SWIM )
	          || ( in_room->sector_type == SECT_UNDERWATER ) )
	    &&   (   ( to_room->sector_type == SECT_WATER_SWIM )
		  || ( to_room->sector_type == SECT_UNDERWATER ) ) )
	    act( "$n swims $T.",  ch, NULL, dir_name[door], TO_ROOM );
	else
	    act( "$n leaves $T.", ch, NULL, dir_name[door], TO_ROOM );
    }
    char_from_room( ch );
    char_to_room( ch, to_room );
    if ( !IS_AFFECTED( ch, AFF_SNEAK )
	&& ( IS_NPC( ch ) || !IS_SET( ch->act, PLR_WIZINVIS ) ) )
	act( "$n has arrived.", ch, NULL, NULL, TO_ROOM );

    /* Because of the addition of the deleted flag, we can do this -Kahn */
    if ( !IS_IMMORTAL( ch ) && ch->race == race_lookup( "vampire" )
	&& to_room->sector_type == SECT_UNDERWATER )
    {
	send_to_char( "Arrgh!  Large body of water!\r\n", ch );
	act( "$n thrashes underwater!", ch, NULL, NULL, TO_ROOM );
	damage( ch, ch, 20, TYPE_UNDEFINED, WEAR_NONE );
    }
    else if ( !IS_IMMORTAL( ch )
	     && ( to_room->sector_type == SECT_UNDERWATER
		 && !IS_AFFECTED( ch, AFF_GILLS )
		 && !IS_SET( race_table[ ch->race ].race_abilities,
			    RACE_WATERBREATH ) ) )
    {
	send_to_char( "You can't breathe!\r\n", ch );
	act( "$n sputters and chokes!", ch, NULL, NULL, TO_ROOM );
	damage( ch, ch, 2, TYPE_UNDEFINED, WEAR_NONE );
    }

    /*
     * Suggested by D'Sai from A Moment in Tyme Mud.  Why have mobiles
     * see the room?  -Kahn
     */
    if ( ch->desc )
    {
	do_look( ch, "auto" );
    }

    SET_BIT( ch->act, moved );

    for ( fch = in_room->people; fch; fch = fch_next )
    {
        fch_next = fch->next_in_room;

        if ( fch->deleted )
	    continue;
      
	if ( fch->master == ch && fch->position == POS_STANDING )
	{
	    act( "You follow $N.", fch, NULL, ch, TO_CHAR );
	    move_char( fch, door );
	}
    }

    REMOVE_BIT( ch->act, moved );
    return;
}



void do_north( CHAR_DATA *ch, char *argument )
{
    move_char( ch, DIR_NORTH );
    return;
}



void do_east( CHAR_DATA *ch, char *argument )
{
    move_char( ch, DIR_EAST );
    return;
}



void do_south( CHAR_DATA *ch, char *argument )
{
    move_char( ch, DIR_SOUTH );
    return;
}



void do_west( CHAR_DATA *ch, char *argument )
{
    move_char( ch, DIR_WEST );
    return;
}



void do_up( CHAR_DATA *ch, char *argument )
{
    move_char( ch, DIR_UP );
    return;
}



void do_down( CHAR_DATA *ch, char *argument )
{
    move_char( ch, DIR_DOWN );
    return;
}



int find_door( CHAR_DATA *ch, char *arg )
{
    EXIT_DATA *pexit;
    int        door;

	 if ( !str_prefix( arg, "north" ) ) door = 0;
    else if ( !str_prefix( arg, "east"  ) ) door = 1;
    else if ( !str_prefix( arg, "south" ) ) door = 2;
    else if ( !str_prefix( arg, "west"  ) ) door = 3;
    else if ( !str_prefix( arg, "up"    ) ) door = 4;
    else if ( !str_prefix( arg, "down"  ) ) door = 5;
    else
    {
	for ( door = 0; door <= 5; door++ )
	{
	    if ( ( pexit = ch->in_room->exit[door] )
		&& IS_SET( pexit->exit_info, EX_ISDOOR )
		&& pexit->keyword
		&& is_name( arg, pexit->keyword ) )
		return door;
	}
	act( "I see no $T here.", ch, NULL, arg, TO_CHAR );
	return -1;
    }

    if ( !( pexit = ch->in_room->exit[door] ) )
    {
	act( "I see no door $T here.", ch, NULL, arg, TO_CHAR );
	return -1;
    }

    if ( !IS_SET( pexit->exit_info, EX_ISDOOR ) )
    {
	send_to_char( "You can't do that.\r\n", ch );
	return -1;
    }

    return door;
}



void do_open( CHAR_DATA *ch, char *argument )
{
    OBJ_DATA *obj;
    char      arg [ MAX_INPUT_LENGTH ];
    int       door;

    one_argument( argument, arg );

    if ( arg[0] == '\0' )
    {
	send_to_char( "Open what?\r\n", ch );
	return;
    }

    if ( ( obj = get_obj_here( ch, arg ) ) )
    {
	/* 'open object' */
	if ( obj->item_type != ITEM_CONTAINER )
	    { send_to_char( "That's not a container.\r\n", ch ); return; }
	if ( !IS_SET( obj->value[1], CONT_CLOSED )    )
	    { send_to_char( "It's already open.\r\n",      ch ); return; }
	if ( !IS_SET( obj->value[1], CONT_CLOSEABLE ) )
	    { send_to_char( "You can't do that.\r\n",      ch ); return; }
	if (  IS_SET( obj->value[1], CONT_LOCKED )    )
	    { send_to_char( "It's locked.\r\n",            ch ); return; }

	REMOVE_BIT( obj->value[1], CONT_CLOSED );
	send_to_char( "Ok.\r\n", ch );
	act( "$n opens $p.", ch, obj, NULL, TO_ROOM );
	return;
    }

    if ( ( door = find_door( ch, arg ) ) >= 0 )
    {
	/* 'open door' */
	EXIT_DATA       *pexit;
	EXIT_DATA       *pexit_rev;
	ROOM_INDEX_DATA *to_room;

	pexit = ch->in_room->exit[door];
	if ( !IS_SET( pexit->exit_info, EX_CLOSED )  )
	    { send_to_char( "It's already open.\r\n",     ch ); return; }
	if (  IS_SET( pexit->exit_info, EX_LOCKED )  )
	    { send_to_char( "It's locked.\r\n",           ch ); return; }

	REMOVE_BIT( pexit->exit_info, EX_CLOSED );
	act( "$n opens the $d.", ch, NULL, pexit->keyword, TO_ROOM );
	send_to_char( "Ok.\r\n", ch );

	/* open the other side */
	if (   ( to_room   = pexit->to_room               )
	    && ( pexit_rev = to_room->exit[rev_dir[door]] )
	    && pexit_rev->to_room == ch->in_room )
	{
	    CHAR_DATA *rch;

	    REMOVE_BIT( pexit_rev->exit_info, EX_CLOSED );
	    for ( rch = to_room->people; rch; rch = rch->next_in_room )
	    {
		if ( rch->deleted )
		    continue;
		act( "The $d opens.", rch, NULL, pexit_rev->keyword, TO_CHAR );
	    }
	}
    }

    return;
}



void do_close( CHAR_DATA *ch, char *argument )
{
    OBJ_DATA *obj;
    char      arg [ MAX_INPUT_LENGTH ];
    int       door;

    one_argument( argument, arg );

    if ( arg[0] == '\0' )
    {
	send_to_char( "Close what?\r\n", ch );
	return;
    }

    if ( ( obj = get_obj_here( ch, arg ) ) )
    {
	/* 'close object' */
	if ( obj->item_type != ITEM_CONTAINER )
	    { send_to_char( "That's not a container.\r\n", ch ); return; }
	if (  IS_SET( obj->value[1], CONT_CLOSED )    )
	    { send_to_char( "It's already closed.\r\n",    ch ); return; }
	if ( !IS_SET( obj->value[1], CONT_CLOSEABLE ) )
	    { send_to_char( "You can't do that.\r\n",      ch ); return; }

	SET_BIT( obj->value[1], CONT_CLOSED );
	send_to_char( "Ok.\r\n", ch );
	act( "$n closes $p.", ch, obj, NULL, TO_ROOM );
	return;
    }

    if ( ( door = find_door( ch, arg ) ) >= 0 )
    {
	/* 'close door' */
	EXIT_DATA       *pexit;
	EXIT_DATA       *pexit_rev;
	ROOM_INDEX_DATA *to_room;

	pexit	= ch->in_room->exit[door];
	if ( IS_SET( pexit->exit_info, EX_CLOSED ) )
	{
	    send_to_char( "It's already closed.\r\n",    ch );
	    return;
	}

	if ( IS_SET( pexit->exit_info, EX_BASHED ) )
	{
	    act( "The $d has been bashed open and cannot be closed.",
		ch, NULL, pexit->keyword, TO_CHAR );
	    return;
	}

	SET_BIT( pexit->exit_info, EX_CLOSED );
	act( "$n closes the $d.", ch, NULL, pexit->keyword, TO_ROOM );
	send_to_char( "Ok.\r\n", ch );

	/* close the other side */
	if (   ( to_room   = pexit->to_room               )
	    && ( pexit_rev = to_room->exit[rev_dir[door]] )
	    && pexit_rev->to_room == ch->in_room )
	{
	    CHAR_DATA *rch;

	    SET_BIT( pexit_rev->exit_info, EX_CLOSED );
	    for ( rch = to_room->people; rch; rch = rch->next_in_room )
	    {
		if ( rch->deleted )
		    continue;
		act( "The $d closes.", rch, NULL, pexit_rev->keyword, TO_CHAR );
	    }
	}
    }

    return;
}



bool has_key( CHAR_DATA *ch, int key )
{
    OBJ_DATA *obj;

    for ( obj = ch->carrying; obj; obj = obj->next_content )
    {
	if ( obj->pIndexData->vnum == key )
	    return TRUE;
    }

    return FALSE;
}



void do_lock( CHAR_DATA *ch, char *argument )
{
    OBJ_DATA *obj;
    char      arg [ MAX_INPUT_LENGTH ];
    int       door;

    one_argument( argument, arg );

    if ( arg[0] == '\0' )
    {
	send_to_char( "Lock what?\r\n", ch );
	return;
    }

    if ( ( obj = get_obj_here( ch, arg ) ) )
    {
	/* 'lock object' */
	if ( obj->item_type != ITEM_CONTAINER )
	    { send_to_char( "That's not a container.\r\n", ch ); return; }
	if ( !IS_SET( obj->value[1], CONT_CLOSED ) )
	    { send_to_char( "It's not closed.\r\n",        ch ); return; }
	if ( obj->value[2] < 0 )
	    { send_to_char( "It can't be locked.\r\n",     ch ); return; }
	if ( !has_key( ch, obj->value[2] ) )
	    { send_to_char( "You lack the key.\r\n",       ch ); return; }
	if (  IS_SET( obj->value[1], CONT_LOCKED ) )
	    { send_to_char( "It's already locked.\r\n",    ch ); return; }

	SET_BIT( obj->value[1], CONT_LOCKED );
	send_to_char( "*Click*\r\n", ch );
	act( "$n locks $p.", ch, obj, NULL, TO_ROOM );
	return;
    }

    if ( ( door = find_door( ch, arg ) ) >= 0 )
    {
	/* 'lock door' */
	EXIT_DATA       *pexit;
	EXIT_DATA       *pexit_rev;
	ROOM_INDEX_DATA *to_room;

	pexit	= ch->in_room->exit[door];
	if ( !IS_SET( pexit->exit_info, EX_CLOSED ) )
	    { send_to_char( "It's not closed.\r\n",        ch ); return; }
	if ( pexit->key < 0 )
	    { send_to_char( "It can't be locked.\r\n",     ch ); return; }
	if ( !has_key( ch, pexit->key ) )
	    { send_to_char( "You lack the key.\r\n",       ch ); return; }
	if (  IS_SET( pexit->exit_info, EX_LOCKED ) )
	    { send_to_char( "It's already locked.\r\n",    ch ); return; }

	SET_BIT( pexit->exit_info, EX_LOCKED );
	send_to_char( "*Click*\r\n", ch );
	act( "$n locks the $d.", ch, NULL, pexit->keyword, TO_ROOM );

	/* lock the other side */
	if (   ( to_room   = pexit->to_room               )
	    && ( pexit_rev = to_room->exit[rev_dir[door]] )
	    && pexit_rev->to_room == ch->in_room )
	{
	    SET_BIT( pexit_rev->exit_info, EX_LOCKED );
	}
    }

    return;
}



void do_unlock( CHAR_DATA *ch, char *argument )
{
    OBJ_DATA *obj;
    char      arg [ MAX_INPUT_LENGTH ];
    int       door;

    one_argument( argument, arg );

    if ( arg[0] == '\0' )
    {
	send_to_char( "Unlock what?\r\n", ch );
	return;
    }

    if ( ( obj = get_obj_here( ch, arg ) ) )
    {
	/* 'unlock object' */
	if ( obj->item_type != ITEM_CONTAINER )
	    { send_to_char( "That's not a container.\r\n", ch ); return; }
	if ( !IS_SET( obj->value[1], CONT_CLOSED ) )
	    { send_to_char( "It's not closed.\r\n",        ch ); return; }
	if ( obj->value[2] < 0 )
	    { send_to_char( "It can't be unlocked.\r\n",   ch ); return; }
	if ( !has_key( ch, obj->value[2] ) )
	    { send_to_char( "You lack the key.\r\n",       ch ); return; }
	if ( !IS_SET( obj->value[1], CONT_LOCKED ) )
	    { send_to_char( "It's already unlocked.\r\n",  ch ); return; }

	REMOVE_BIT( obj->value[1], CONT_LOCKED );
	send_to_char( "*Click*\r\n", ch );
	act( "$n unlocks $p.", ch, obj, NULL, TO_ROOM );
	return;
    }

    if ( ( door = find_door( ch, arg ) ) >= 0 )
    {
	/* 'unlock door' */
	EXIT_DATA       *pexit;
	EXIT_DATA       *pexit_rev;
	ROOM_INDEX_DATA *to_room;

	pexit = ch->in_room->exit[door];
	if ( !IS_SET( pexit->exit_info, EX_CLOSED ) )
	    { send_to_char( "It's not closed.\r\n",        ch ); return; }
	if ( pexit->key < 0 )
	    { send_to_char( "It can't be unlocked.\r\n",   ch ); return; }
	if ( !has_key( ch, pexit->key ) )
	    { send_to_char( "You lack the key.\r\n",       ch ); return; }
	if ( !IS_SET( pexit->exit_info, EX_LOCKED ) )
	    { send_to_char( "It's already unlocked.\r\n",  ch ); return; }

	REMOVE_BIT( pexit->exit_info, EX_LOCKED );
	send_to_char( "*Click*\r\n", ch );
	act( "$n unlocks the $d.", ch, NULL, pexit->keyword, TO_ROOM );

	/* unlock the other side */
	if (   ( to_room   = pexit->to_room               )
	    && ( pexit_rev = to_room->exit[rev_dir[door]] )
	    && pexit_rev->to_room == ch->in_room )
	{
	    REMOVE_BIT( pexit_rev->exit_info, EX_LOCKED );
	}
    }

    return;
}



void do_pick( CHAR_DATA *ch, char *argument )
{
    OBJ_DATA  *obj;
    CHAR_DATA *gch;
    char       arg [ MAX_INPUT_LENGTH ];
    int        door;

    one_argument( argument, arg );

    if ( arg[0] == '\0' )
    {
	send_to_char( "Pick what?\r\n", ch );
	return;
    }

    WAIT_STATE( ch, skill_table[gsn_pick_lock].beats );

    /* look for guards */
    for ( gch = ch->in_room->people; gch; gch = gch->next_in_room )
    {
        if ( gch->deleted )
	    continue;
	if ( IS_NPC( gch ) && IS_AWAKE( gch ) && ch->level + 5 < gch->level )
	{
	    act( "$N is standing too close to the lock.",
		ch, NULL, gch, TO_CHAR );
	    return;
	}
    }

    /* Check skill roll for player-char, make sure mob isn't charmed */
    if ( (  !IS_NPC( ch )
	  && number_percent( ) > ch->pcdata->learned[gsn_pick_lock] )
	|| ( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) ) )
    {
	send_to_char( "You failed.\r\n", ch);
	return;
    }

    if ( ( obj = get_obj_here( ch, arg ) ) )
    {
	/* 'pick object' */
	if ( obj->item_type != ITEM_CONTAINER )
	    { send_to_char( "That's not a container.\r\n", ch ); return; }
	if ( !IS_SET( obj->value[1], CONT_CLOSED )    )
	    { send_to_char( "It's not closed.\r\n",        ch ); return; }
	if ( obj->value[2] < 0 )
	    { send_to_char( "It can't be unlocked.\r\n",   ch ); return; }
	if ( !IS_SET( obj->value[1], CONT_LOCKED )    )
	    { send_to_char( "It's already unlocked.\r\n",  ch ); return; }
	if (  IS_SET( obj->value[1], CONT_PICKPROOF ) )
	    { send_to_char( "You failed.\r\n",             ch ); return; }

	REMOVE_BIT( obj->value[1], CONT_LOCKED );
	send_to_char( "*Click*\r\n", ch );
	act( "$n picks $p.", ch, obj, NULL, TO_ROOM );
	return;
    }

    if ( ( door = find_door( ch, arg ) ) >= 0 )
    {
	/* 'pick door' */
	EXIT_DATA       *pexit;
	EXIT_DATA       *pexit_rev;
	ROOM_INDEX_DATA *to_room;

	pexit = ch->in_room->exit[door];
	if ( !IS_SET( pexit->exit_info, EX_CLOSED )    )
	    { send_to_char( "It's not closed.\r\n",        ch ); return; }
	if ( pexit->key < 0 )
	    { send_to_char( "It can't be picked.\r\n",     ch ); return; }
	if ( !IS_SET( pexit->exit_info, EX_LOCKED )    )
	    { send_to_char( "It's already unlocked.\r\n",  ch ); return; }
	if (  IS_SET( pexit->exit_info, EX_PICKPROOF ) )
	    { send_to_char( "You failed.\r\n",             ch ); return; }

	REMOVE_BIT( pexit->exit_info, EX_LOCKED );
	send_to_char( "*Click*\r\n", ch );
	act( "$n picks the $d.", ch, NULL, pexit->keyword, TO_ROOM );

	/* pick the other side */
	if (   ( to_room   = pexit->to_room               )
	    && ( pexit_rev = to_room->exit[rev_dir[door]] )
	    && pexit_rev->to_room == ch->in_room )
	{
	    REMOVE_BIT( pexit_rev->exit_info, EX_LOCKED );
	}
    }

    return;
}




void do_stand( CHAR_DATA *ch, char *argument )
{
    switch ( ch->position )
    {
    case POS_SLEEPING:
	if ( IS_AFFECTED( ch, AFF_SLEEP ) )
	    { send_to_char( "You can't wake up!\r\n", ch ); return; }

	send_to_char( "You wake and stand up.\r\n", ch );
	act( "$n wakes and stands up.", ch, NULL, NULL, TO_ROOM );
	ch->position = POS_STANDING;
	break;

    case POS_RESTING:
	send_to_char( "You stand up.\r\n", ch );
	act( "$n stands up.", ch, NULL, NULL, TO_ROOM );
	ch->position = POS_STANDING;
	break;

    case POS_FIGHTING:
	send_to_char( "You are already fighting!\r\n",  ch );
	break;

    case POS_STANDING:
	send_to_char( "You are already standing.\r\n",  ch );
	break;
    }

    return;
}



void do_rest( CHAR_DATA *ch, char *argument )
{
    switch ( ch->position )
    {
    case POS_SLEEPING:
	send_to_char( "You are already sleeping.\r\n",  ch );
	break;

    case POS_RESTING:
	send_to_char( "You are already resting.\r\n",   ch );
	break;

    case POS_FIGHTING:
	send_to_char( "Not while you're fighting!\r\n", ch );
	break;

    case POS_STANDING:
	send_to_char( "You rest.\r\n", ch );
	act( "$n rests.", ch, NULL, NULL, TO_ROOM );
	ch->position = POS_RESTING;
	break;
    }

    return;
}



void do_sleep( CHAR_DATA *ch, char *argument )
{
    switch ( ch->position )
    {
    case POS_SLEEPING:
	send_to_char( "You are already sleeping.\r\n",  ch );
	break;

    case POS_RESTING:
    case POS_STANDING: 
	send_to_char( "You sleep.\r\n", ch );
	act( "$n sleeps.", ch, NULL, NULL, TO_ROOM );
	ch->position = POS_SLEEPING;
	break;

    case POS_FIGHTING:
	send_to_char( "Not while you're fighting!\r\n", ch );
	break;
    }

    return;
}



void do_wake( CHAR_DATA *ch, char *argument )
{
    CHAR_DATA *victim;
    char       arg [ MAX_INPUT_LENGTH ];

    one_argument( argument, arg );
    if ( arg[0] == '\0' )
	{ do_stand( ch, argument ); return; }

    if ( !IS_AWAKE( ch ) )
	{ send_to_char( "You are asleep yourself!\r\n",       ch ); return; }

    if ( !( victim = get_char_room( ch, arg ) ) )
	{ send_to_char( "They aren't here.\r\n",              ch ); return; }

    if ( IS_AWAKE( victim ) )
	{ act( "$N is already awake.", ch, NULL, victim, TO_CHAR ); return; }

    if ( IS_AFFECTED( victim, AFF_SLEEP ) )
	{ act( "You can't wake $M!",   ch, NULL, victim, TO_CHAR ); return; }

    victim->position = POS_STANDING;
    act( "You wake $M.",  ch, NULL, victim, TO_CHAR );
    act( "$n wakes you.", ch, NULL, victim, TO_VICT );
    return;
}



void do_sneak( CHAR_DATA *ch, char *argument )
{
    AFFECT_DATA af;

    /* Don't allow charmed mobs to do this, check player's skill */
    if ( ( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
	|| ( !IS_NPC( ch )
	    && ch->level < skill_table[gsn_sneak].skill_level[ch->class] ) )
    {
        send_to_char( "Huh?\r\n", ch );
	return;
    }

    send_to_char( "You attempt to move silently.\r\n", ch );
    affect_strip( ch, gsn_sneak );

    if ( IS_NPC( ch ) || number_percent( ) < ch->pcdata->learned[gsn_sneak] )
    {
	af.type      = gsn_sneak;
	af.duration  = ch->level;
	af.location  = APPLY_NONE;
	af.modifier  = 0;
	af.bitvector = AFF_SNEAK;
	affect_to_char( ch, &af );
    }

    return;
}



void do_hide( CHAR_DATA *ch, char *argument )
{
    /* Dont allow charmed mobiles to do this, check player's skill */
    if ( ( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
	|| ( !IS_NPC( ch )
	    && ch->level < skill_table[gsn_hide].skill_level[ch->class] ) )
    {
        send_to_char( "Huh?\r\n", ch );
	return;
    }

    send_to_char( "You attempt to hide.\r\n", ch );

    if ( IS_AFFECTED( ch, AFF_HIDE ) )
	REMOVE_BIT( ch->affected_by, AFF_HIDE);

    if ( IS_NPC( ch ) || number_percent( ) < ch->pcdata->learned[gsn_hide] )
	SET_BIT( ch->affected_by, AFF_HIDE);

    return;
}



/*
 * Contributed by Alander.
 */
void do_visible( CHAR_DATA *ch, char *argument )
{
    affect_strip ( ch, gsn_invis			);
    affect_strip ( ch, gsn_mass_invis			);
    affect_strip ( ch, gsn_sneak			);
    affect_strip ( ch, gsn_shadow                       );
    REMOVE_BIT   ( ch->affected_by, AFF_HIDE		);
    REMOVE_BIT   ( ch->affected_by, AFF_INVISIBLE	);
    REMOVE_BIT   ( ch->affected_by, AFF_SNEAK		);
    send_to_char( "Ok.\r\n", ch );
    return;
}



void do_recall( CHAR_DATA *ch, char *argument )
{
    CHAR_DATA       *victim;
    ROOM_INDEX_DATA *location;
    char             buf [ MAX_STRING_LENGTH ];
    int              place;

    act( "$n prays for transportation!", ch, NULL, NULL, TO_ROOM );

    if ( IS_SET( ch->in_room->room_flags, ROOM_NO_RECALL )
	|| IS_AFFECTED( ch, AFF_CURSE ) )
    {
	send_to_char( "God has forsaken you.\r\n", ch );
	return;
    }

    place = ch->in_room->area->recall;
    if ( !( location = get_room_index( place ) ) )
    {
	send_to_char( "You are completely lost.\r\n", ch );
	return;
    }

    if ( ch->in_room == location )
	return;

    if ( ( victim = ch->fighting ) )
    {
	int lose;

	if ( number_bits( 1 ) == 0 )
	{
	    WAIT_STATE( ch, 4 );
	    lose = ( ch->desc ) ? 25 : 50;
	    gain_exp( ch, 0 - lose );
	    sprintf( buf, "You failed!  You lose %d exps.\r\n", lose );
	    send_to_char( buf, ch );
	    return;
	}

	lose = ( ch->desc ) ? 50 : 100;
	gain_exp( ch, 0 - lose );
	sprintf( buf, "You recall from combat!  You lose %d exps.\r\n", lose );
	send_to_char( buf, ch );
	stop_fighting( ch, TRUE );
    }

    ch->move /= 2;
    act( "$n disappears.", ch, NULL, NULL, TO_ROOM );
    char_from_room( ch );
    char_to_room( ch, location );
    act( "$n appears in the room.", ch, NULL, NULL, TO_ROOM );
    do_look( ch, "auto" );

    return;
}



void do_train( CHAR_DATA *ch, char *argument )
{
    CHAR_DATA *mob;
    char      *pOutput;
    char       buf [ MAX_STRING_LENGTH ];
    bool       ok = FALSE;
    int       *pAbility;
    int        cost;
    int        money;
    int        bone_flag = 0; /*Added for training of hp ma mv */

    if ( IS_NPC( ch ) )
	return;

    /*
     * Check for trainer.
     */
    for ( mob = ch->in_room->people; mob; mob = mob->next_in_room )
    {
	if ( IS_NPC( mob ) && IS_SET( mob->act, ACT_TRAIN ) )
	    break;
    }

    if ( !mob )
    {
	send_to_char( "You can't do that here.\r\n", ch );
	return;
    }

    if ( argument[0] == '\0' )
    {
	sprintf( buf, "You have %d practice sessions.\r\n", ch->practice );
	send_to_char( buf, ch );
	argument = "foo";
    }

    cost  = 5;
    money = ch->level * ch->level * 100;

    if ( !str_cmp( argument, "str" ) )
    {
	if ( class_table[ch->class].attr_prime == APPLY_STR )
	    cost    = 3;
	pAbility    = &ch->pcdata->perm_str;
	pOutput     = "strength";
    }

    else if ( !str_cmp( argument, "int" ) )
    {
	if ( class_table[ch->class].attr_prime == APPLY_INT )
	    cost    = 3;
	pAbility    = &ch->pcdata->perm_int;
	pOutput     = "intelligence";
    }

    else if ( !str_cmp( argument, "wis" ) )
    {
	if ( class_table[ch->class].attr_prime == APPLY_WIS )
	    cost    = 3;
	pAbility    = &ch->pcdata->perm_wis;
	pOutput     = "wisdom";
    }

    else if ( !str_cmp( argument, "dex" ) )
    {
	if ( class_table[ch->class].attr_prime == APPLY_DEX )
	    cost    = 3;
	pAbility    = &ch->pcdata->perm_dex;
	pOutput     = "dexterity";
    }

    else if ( !str_cmp( argument, "con" ) )
    {
	if ( class_table[ch->class].attr_prime == APPLY_CON )
	    cost    = 3;
	pAbility    = &ch->pcdata->perm_con;
	pOutput     = "constitution";
    }

    /* ---------------- By Bonecrusher ------------------- */

    else if ( !str_cmp( argument, "hp" ) )
    {
 	    cost    = 1;
	    money   = ch->level * ch->level * 20;
	bone_flag   = 1;
        pAbility    = &ch->max_hit;
        pOutput     = "hit points";
    }
	    
    else if ( !str_cmp( argument, "mana" ) )
    {
 	    cost    = 1;
	    money   = ch->level * ch->level * 20;
	bone_flag   = 1;
        pAbility    = &ch->max_mana;
        pOutput     = "mana points";
    }

    else if ( !str_cmp( argument, "move" ) )
    {
 	    cost    = 1;
	    money   = ch->level * ch->level * 20;
	bone_flag   = 2;
        pAbility    = &ch->max_move;
        pOutput     = "move points";
    }

    /* --------------------------------------------*/

    else
    {
	strcpy( buf, "You can train:" );
	if ( ch->pcdata->perm_str < 18 + race_table[ ch->race ].str_mod )
	    strcat( buf, " str" );
	if ( ch->pcdata->perm_int < 18 + race_table[ ch->race ].int_mod )
	    strcat( buf, " int" );
	if ( ch->pcdata->perm_wis < 18 + race_table[ ch->race ].wis_mod )
	    strcat( buf, " wis" );
	if ( ch->pcdata->perm_dex < 18 + race_table[ ch->race ].dex_mod )
	    strcat( buf, " dex" );
	if ( ch->pcdata->perm_con < 18 + race_table[ ch->race ].con_mod )
	    strcat( buf, " con" );

	strcat( buf, " hp mana move" );

	if ( buf[strlen( buf )-1] != ':' )
	{
	    strcat( buf, ".\r\n" );
	    send_to_char( buf, ch );
	    sprintf( buf, "Cost is %d gold coins for attributes.\r\n", money );
	    send_to_char( buf, ch );
	    money   = ch->level * ch->level * 20;
	    sprintf( buf, "Cost is %d gold coins per hp/mana/move.\r\n",
		    money );
	    send_to_char( buf, ch );
	}

	return;
    }

    if ( bone_flag == 0 )
      {
	if ( !str_cmp( argument, "str" ) )
	{
	    if ( *pAbility < 18 + race_table[ ch->race ].str_mod )
	      {
	        ok = TRUE;
	      }
	}
	else if ( !str_cmp( argument, "int" ) )
	{ 
	    if ( *pAbility < 18 + race_table[ ch->race ].int_mod )
	      {
	        ok = TRUE;
	      }
	}
	else if ( !str_cmp( argument, "wis" ) )
	{
	    if ( *pAbility < 18 + race_table[ ch->race ].wis_mod )
	      {
		ok = TRUE;
	      }
	}
	else if ( !str_cmp( argument, "dex" ) )
	{
	    if ( *pAbility < 18 + race_table[ ch->race ].dex_mod )
	      {
	        ok = TRUE;
	      }
	}
	else if ( !str_cmp( argument, "con" ) )
	{
	    if ( *pAbility < 18 + race_table[ ch->race ].con_mod )
	      {
	        ok = TRUE;
	      }
	}

	if ( !ok )
	{
	    act( "Your $T is already at maximum.",
		ch, NULL, pOutput, TO_CHAR );
	    return;
	}
    }

    if ( cost > ch->practice )
    {
	send_to_char( "You don't have enough practices.\r\n", ch );
	return;
    }
    else if ( money > ch->gold )
    {
	send_to_char( "You don't have enough money.\r\n", ch );
	return;
    }

    ch->practice        	-= cost;
    ch->gold                    -= money;

    if ( bone_flag == 0 )
        *pAbility		+= 1;
    else if ( bone_flag == 1 )
        *pAbility               += dice( 1, 2 );
    else
        *pAbility               += dice( 1, 5 );

    if ( bone_flag == 0 )
    {
        act( "Your $T increases!", ch, NULL, pOutput, TO_CHAR );
        act( "$n's $T increases!", ch, NULL, pOutput, TO_ROOM );
	return;
    }

    act( "Your $T increase!", ch, NULL, pOutput, TO_CHAR );
    act( "$n's $T increase!", ch, NULL, pOutput, TO_ROOM );

    return;
}

void do_chameleon ( CHAR_DATA *ch, char *argument )
{
    if ( !IS_NPC( ch )
	&& ch->level < skill_table[gsn_chameleon].skill_level[ch->class] )
    {
        send_to_char( "Huh?\r\n", ch );
	return;
    }

    send_to_char( "You attempt to blend in with your surroundings.\r\n", ch);

    if ( IS_AFFECTED( ch, AFF_HIDE ) )
        REMOVE_BIT( ch->affected_by, AFF_HIDE );

    if ( IS_NPC( ch ) || number_percent( ) < ch->pcdata->learned[gsn_chameleon] )
        SET_BIT( ch->affected_by, AFF_HIDE );

    return;
}

void do_heighten ( CHAR_DATA *ch, char *argument )
{
    AFFECT_DATA af;

    if ( !IS_NPC( ch )
	&& ch->level < skill_table[gsn_heighten].skill_level[ch->class] )
    {
        send_to_char( "Huh?\r\n", ch );
	return;
    }

    if ( is_affected( ch, gsn_heighten ) )
        return;

    if ( IS_NPC( ch ) || number_percent( ) < ch->pcdata->learned[gsn_heighten] )
    {
	af.type      = gsn_heighten;
	af.duration  = 24;
	af.modifier  = 0;
	af.location  = APPLY_NONE;
	af.bitvector = AFF_DETECT_INVIS;
	affect_to_char( ch, &af );

	af.bitvector = AFF_DETECT_HIDDEN;
	affect_to_char( ch, &af );
	
	af.bitvector = AFF_INFRARED;
	affect_to_char( ch, &af );
	
	send_to_char( "Your senses are heightened.\r\n", ch );
    }
    return;

}

void do_shadow ( CHAR_DATA *ch, char *argument )
{
    AFFECT_DATA af;

    if ( !IS_NPC( ch )
	&& ch->level < skill_table[gsn_shadow].skill_level[ch->class] )
    {
        send_to_char( "Huh?\r\n", ch );
	return;
    }

    send_to_char( "You attempt to move in the shadows.\r\n", ch );
    affect_strip( ch, gsn_shadow );

    if ( IS_NPC( ch ) || number_percent( ) < ch->pcdata->learned[gsn_shadow] )
    {
	af.type      = gsn_shadow;
	af.duration  = ch->level;
	af.modifier  = APPLY_NONE;
	af.location  = 0;
	af.bitvector = AFF_SNEAK;
	affect_to_char( ch, &af );
    }
    return;

}

/*
 * Bash code by Thelonius for EnvyMud (originally bash_door)
 * Damage modified using Morpheus's code
 * Message for bashproof doors by that wacky guy Kahn
 */
void do_bash( CHAR_DATA *ch, char *argument )
{
    CHAR_DATA *gch;
    char       arg [ MAX_INPUT_LENGTH ];
    int        door;

    if ( IS_NPC( ch ) || ( !IS_NPC( ch )
			  && ch->level
			  < skill_table[gsn_bash].skill_level[ch->class] ) )
    {
	send_to_char( "You're not enough of a warrior to bash doors!\r\n",
		     ch );
	return;
    }

    one_argument( argument, arg );

    if ( arg[0] == '\0' )
    {
	send_to_char( "Bash what?\r\n", ch );
	return;
    }

    if ( ch->fighting )
    {
	send_to_char( "You can't break off your fight.\r\n", ch );
	return;
    }

    if ( ( door = find_door( ch, arg ) ) >= 0 )
    {
	ROOM_INDEX_DATA *to_room;
	EXIT_DATA       *pexit;
	EXIT_DATA       *pexit_rev;
	int              chance;

	pexit = ch->in_room->exit[door];
	if ( !IS_SET( pexit->exit_info, EX_CLOSED ) )
	{
	    send_to_char( "Calm down.  It is already open.\r\n", ch );
	    return;
	}

	WAIT_STATE( ch, skill_table[gsn_bash].beats );

	if ( !IS_NPC( ch ) )
	  chance = ch->pcdata->learned[gsn_bash]/2;
	else
	  chance = 0;

	if ( IS_SET( pexit->exit_info, EX_LOCKED ) )
	    chance /= 2;

	if ( IS_SET( pexit->exit_info, EX_BASHPROOF ) )
	{
	    act( "WHAAAAM!!!  You bash against the $d, but it doesn't budge.",
		ch, NULL, pexit->keyword, TO_CHAR );
	    act( "WHAAAAM!!!  $n bashes against the $d, but it holds strong.",
		ch, NULL, pexit->keyword, TO_ROOM );
	    damage( ch, ch, ( ch->max_hit /  5 ), gsn_bash, WEAR_NONE );
	    return;
	}

	if ( ( get_curr_str( ch ) >= 20 )
	    && number_percent( ) <
	        ( chance + 4 * ( get_curr_str( ch ) - 20 ) ) )
	{
	    /* Success */

	    REMOVE_BIT( pexit->exit_info, EX_CLOSED );
	    if ( IS_SET( pexit->exit_info, EX_LOCKED ) )
	        REMOVE_BIT( pexit->exit_info, EX_LOCKED );
	    
	    SET_BIT( pexit->exit_info, EX_BASHED );

	    act( "Crash!  You bashed open the $d!",
		ch, NULL, pexit->keyword, TO_CHAR );
	    act( "$n bashes open the $d!",
		ch, NULL, pexit->keyword, TO_ROOM );

	    damage( ch, ch, ( ch->max_hit / 20 ), gsn_bash, WEAR_NONE );

	    /* Bash through the other side */
	    if (   ( to_room   = pexit->to_room               )
		&& ( pexit_rev = to_room->exit[rev_dir[door]] )
		&& pexit_rev->to_room == ch->in_room        )
	    {
		CHAR_DATA *rch;

		REMOVE_BIT( pexit_rev->exit_info, EX_CLOSED );
		if ( IS_SET( pexit_rev->exit_info, EX_LOCKED ) )
		    REMOVE_BIT( pexit_rev->exit_info, EX_LOCKED );

		SET_BIT( pexit_rev->exit_info, EX_BASHED );

		for ( rch = to_room->people; rch; rch = rch->next_in_room )
		{
		    if ( rch->deleted )
		        continue;
		    act( "The $d crashes open!",
			rch, NULL, pexit_rev->keyword, TO_CHAR );
		}

	    }
	}
	else
	{
	    /* Failure */
	    
	    act( "OW!  You bash against the $d, but it doesn't budge.",
		ch, NULL, pexit->keyword, TO_CHAR );
	    act( "$n bashes against the $d, but it holds strong.",
		ch, NULL, pexit->keyword, TO_ROOM );
	    damage( ch, ch, ( ch->max_hit / 10 ), gsn_bash, WEAR_NONE );
	}
    }

    /*
     * Check for "guards"... anyone bashing a door is considered as
     * a potential aggressor, and there's a 25% chance that mobs
     * will do unto before being done unto.
     * But first...let's make sure ch ain't dead?  That'd be a pain.
     */

    if ( ch->deleted || ch->hit <= 1 )
        return;

    for ( gch = ch->in_room->people; gch; gch = gch->next_in_room )
    {
	if ( !gch->deleted
	    && gch != ch
	    && IS_AWAKE( gch )
	    && can_see( gch, ch )
	    && !gch->fighting
	    && ( IS_NPC( gch ) && !IS_AFFECTED( gch, AFF_CHARM ) )
	    && ( ch->level - gch->level <= 4 )
	    && number_bits( 2 ) == 0 )
	    multi_hit( gch, ch, TYPE_UNDEFINED );
    }

    return;

}

/* Snare skill by Binky for EnvyMud */
void do_snare( CHAR_DATA *ch, char *argument )
{
    CHAR_DATA   *victim;
    AFFECT_DATA  af;
    char         arg [ MAX_INPUT_LENGTH ];

    one_argument( argument, arg );

    /*
     *  First, this checks for case of no second argument (valid only
     *  while fighting already).  Later, if an argument is given, it 
     *  checks validity of argument.  Unsuccessful snares flow through
     *  and receive messages at the end of the function.
     */

    if ( arg[0] == '\0' )
    {
	if ( !( victim = ch->fighting ) )
	{
	    send_to_char( "Ensnare whom?\r\n", ch );
	    return;
	}
	/* No argument, but already fighting: valid use of snare */
	WAIT_STATE( ch, skill_table[gsn_snare].beats );

	/* Only appropriately skilled PCs and uncharmed mobs */
	if ( ( IS_NPC( ch ) && !IS_AFFECTED( ch, AFF_CHARM ) )
	    || ( !IS_NPC( ch )
		&& number_percent( ) < ch->pcdata->learned[gsn_snare] ) )
	{    
	    affect_strip( victim, gsn_snare );  

	    af.type      = gsn_snare;
	    af.duration  = 1 + ( ( ch->level ) / 8 );
	    af.location  = APPLY_NONE;
	    af.modifier  = 0;
	    af.bitvector = AFF_HOLD;

	    affect_to_char( victim, &af );

	    act( "You have ensnared $M!", ch, NULL, victim, TO_CHAR    );
	    act( "$n has ensnared you!",  ch, NULL, victim, TO_VICT    );
	    act( "$n has ensnared $N.",   ch, NULL, victim, TO_NOTVICT );
	}
	else
	{
	    act( "You failed to ensnare $M.  Uh oh!",
		ch, NULL, victim, TO_CHAR    );
	    act( "$n tried to ensnare you!  Get $m!",
		ch, NULL, victim, TO_VICT    );
	    act( "$n attempted to ensnare $N, but failed!",
		ch, NULL, victim, TO_NOTVICT );
	}
    }
    else				/* argument supplied */
    {
	if ( !( victim = get_char_room( ch, arg ) ) )
	{
	    send_to_char( "They aren't here.\r\n", ch );
	    return;
	}

	if ( !IS_NPC( ch ) && !IS_NPC( victim ) )
	{
	    send_to_char( "You can't ensnare another player.\r\n", ch );
	    return;
	}

	if ( victim != ch->fighting ) /* TRUE if not fighting, or fighting  */
	{                             /* if person other than victim        */
	    if ( ch->fighting )       /* TRUE if fighting other than vict.  */ 
	    {		
		send_to_char(
		    "Take care of the person you are fighting first!\r\n",
			     ch );
		return;
	    }                             
	    WAIT_STATE( ch, skill_table[gsn_snare].beats );

	    /* here, arg supplied, ch not fighting */
	    /* only appropriately skilled PCs and uncharmed mobs */
	    if ( ( IS_NPC( ch ) && !IS_AFFECTED( ch, AFF_CHARM ) )
		|| ( !IS_NPC( ch )
		    && number_percent( ) < ch->pcdata->learned[gsn_snare] ) )
	    {
		affect_strip( victim, gsn_snare );  

		af.type      = gsn_snare;
		af.duration  = 3 + ( (ch->level ) / 8 );
		af.location  = APPLY_NONE;
		af.modifier  = 0;
		af.bitvector = AFF_HOLD;

		affect_to_char( victim, &af );

		act( "You have ensnared $M!", ch, NULL, victim, TO_CHAR    );
		act( "$n has ensnared you!",  ch, NULL, victim, TO_VICT    );
		act( "$n has ensnared $N.",   ch, NULL, victim, TO_NOTVICT );
	    }
	    else
	    {
		act( "You failed to ensnare $M.  Uh oh!",
		    ch, NULL, victim, TO_CHAR    );
		act( "$n tried to ensnare you!  Get $m!",
		    ch, NULL, victim, TO_VICT    );
		act( "$n attempted to ensnare $N, but failed!",
		    ch, NULL, victim, TO_NOTVICT );
	    }
	    if ( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
	    {
		/* go for the one who wanted to fight :) */
		multi_hit( victim, ch->master, TYPE_UNDEFINED );
	    }
	    else
	    {
	        multi_hit( victim, ch, TYPE_UNDEFINED );
	    }
	}
	else
	{
	    /* we are already fighting the intended victim */
	    WAIT_STATE( ch, skill_table[gsn_snare].beats );

	    /* charmed mobs not allowed to do this */
	    if ( ( IS_NPC( ch ) && !IS_AFFECTED( ch, AFF_CHARM ) )
		|| ( !IS_NPC( ch )
		    && number_percent( ) < ch->pcdata->learned[gsn_snare] ) )
	    {
		affect_strip( victim, gsn_snare );  

		af.type      = gsn_snare;
		af.duration  = 1 + ( ( ch->level ) / 8 );
		af.location  = APPLY_NONE;
		af.modifier  = 0;
		af.bitvector = AFF_HOLD;

		affect_to_char( victim, &af );

		act( "You have ensnared $M!", ch, NULL, victim, TO_CHAR    );
		act( "$n has ensnared you!",  ch, NULL, victim, TO_VICT    );
		act( "$n has ensnared $N.",   ch, NULL, victim, TO_NOTVICT );
	    }
	    else
	    {
		act( "You failed to ensnare $M.  Uh oh!",
		    ch, NULL, victim, TO_CHAR    );
		act( "$n tried to ensnare you!  Get $m!",
		    ch, NULL, victim, TO_VICT    );
		act( "$n attempted to ensnare $N, but failed!",
		    ch, NULL, victim, TO_NOTVICT );
	    }
	}
    }

    return;
}



/* Untangle by Thelonius for EnvyMud */
void do_untangle( CHAR_DATA *ch, char *argument )
{
    CHAR_DATA   *victim;
    char         arg [ MAX_INPUT_LENGTH ];

    if ( !IS_NPC( ch )
	&& ch->level < skill_table[gsn_untangle].skill_level[ch->class] )
    {
	send_to_char( "You aren't nimble enough.\r\n", ch );
        return;
    }

    one_argument( argument, arg );

    if ( arg[0] == '\0' )
	victim = ch;
    else if ( !( victim = get_char_room( ch, arg ) ) )
    {
	    send_to_char( "They aren't here.\r\n", ch );
	    return;
    }

    if ( !is_affected( victim, gsn_snare ) )
	return;

    if ( ( IS_NPC( ch ) && !IS_AFFECTED( ch, AFF_CHARM ) )
	|| ( !IS_NPC( ch ) 
	    && number_percent( ) < ch->pcdata->learned[gsn_untangle] ) )
    {
	affect_strip( victim, gsn_snare );

	if ( victim != ch )
        {
	    act( "You untangle $N.",  ch, NULL, victim, TO_CHAR    );
	    act( "$n untangles you.", ch, NULL, victim, TO_VICT    );
	    act( "$n untangles $n.",  ch, NULL, victim, TO_NOTVICT );
        }
	else
        {
	    send_to_char( "You untangle yourself.\r\n", ch );
	    act( "$n untangles $mself.", ch, NULL, NULL, TO_ROOM );
        }

	return;
    }
}



/*
 *  Menu for all game functions.
 *  Thelonius (Monk)  5/94
 */
void do_bet( CHAR_DATA *ch, char *argument )
{
    CHAR_DATA *croupier;

    if ( IS_AFFECTED( ch, AFF_MUTE )
	|| IS_SET( race_table[ch->race].race_abilities, RACE_MUTE )
        || IS_SET( ch->in_room->room_flags, ROOM_CONE_OF_SILENCE ) )
    {
        send_to_char( "Your lips move but no sound comes out.\r\n", ch );
        return;
    }

    /*
     *  The following searches for a valid croupier.  It allows existing
     *  ACT_GAMBLE mobs to attempt to gamble with other croupiers, but
     *  will not allow them to gamble with themselves (i.e., switched
     *  imms).  This takes care of ch == croupier in later act()'s
     */
    for( croupier = ch->in_room->people;
	croupier;
	croupier = croupier->next_in_room )
    {
	if ( IS_NPC( croupier )
	    && IS_SET( croupier->act, ACT_GAMBLE )
	    && !IS_AFFECTED( croupier, AFF_MUTE )
	    && !IS_SET( race_table[croupier->race].race_abilities, RACE_MUTE )
	    && croupier != ch )
	    break;
    }

    if ( !croupier )
    {
	send_to_char( "You can't gamble here.\r\n", ch );
	return;
    }

    switch( croupier->pIndexData->vnum )
    {
	default:
	    bug( "ACT_GAMBLE set on undefined game; vnum = %d",
		croupier->pIndexData->vnum );
	    break;
	case MOB_VNUM_ULT:
	    game_u_l_t( ch, croupier, argument );
	    break;
    }

    return;
}



/*
 * Upper-Lower-Triple
 * Game idea by Partan
 * Coded by Thelonius
 */
void game_u_l_t( CHAR_DATA *ch, CHAR_DATA *croupier, char *argument )
{
    char msg    [ MAX_STRING_LENGTH ];
    char buf    [ MAX_STRING_LENGTH ];
    char limit  [ MAX_STRING_LENGTH ] = "5000";
    char wager  [ MAX_INPUT_LENGTH  ];
    char choice [ MAX_INPUT_LENGTH  ];
    int  ichoice;
    int  amount;
    int  die1;
    int  die2;
    int  die3;
    int  total;

    argument = one_argument( argument, wager );
    one_argument( argument, choice );

    if ( wager[0] == '\0' || !is_number( wager ) )
    {
	send_to_char( "How much would you like to bet?\r\n", ch );
	return;
    }

    amount = atoi( wager );

    if ( amount > ch->gold )
    {
	send_to_char( "You don't have enough gold!\r\n", ch );
	return;
    }

    if ( amount < 0 )
    {
	send_to_char( "You are unable to wager imaginary gold.\r\n", ch );
	return;
    }
	
    if ( amount > atoi( limit ) )
    {
	act( "$n tells you, 'Sorry, the house limit is $t.'",
	    croupier, limit, ch, TO_VICT );
	ch->reply = croupier;
	return;
    }
/*
 *  At the moment, the winnings (and losses) do not actually go through
 *  the croupier.  They could do so, if each croupier is loaded with a 
 *  certain bankroll.  Unfortunately, they would probably be popular
 *  (and rich) targets.
 */

         if ( !str_cmp( choice, "lower"  ) ) ichoice = 1;
    else if ( !str_cmp( choice, "upper"  ) ) ichoice = 2;
    else if ( !str_cmp( choice, "triple" ) ) ichoice = 3;
    else
    {
	send_to_char( "What do you wish to bet: Upper, Lower, or Triple?\r\n",
		     ch );
	return;
    }
/*
 *  Now we have a wagering amount, and a choice.
 *  Let's place the bets and roll the dice, shall we?
 */
    act( "You place $t gold coins on the table, and bet '$T'.",
	ch, wager, choice,   TO_CHAR    );
    act( "$n places a bet with you.",
	ch, NULL,  croupier, TO_VICT    );
    act( "$n plays a dice game.",
	ch, NULL,  croupier, TO_NOTVICT );
    ch->gold -= amount;

    die1 = number_range( 1, 6 );
    die2 = number_range( 1, 6 );
    die3 = number_range( 1, 6 );
    total = die1 + die2 + die3;

    sprintf( msg, "$n rolls the dice: they come up %d, %d, and %d",
	    die1, die2, die3 );

    if( die1 == die2 && die2 == die3 )
    {
	strcat( msg, "." );
	act( msg, croupier, NULL, ch, TO_VICT );

	if ( ichoice == 3 )
	{
	    char haul [ MAX_STRING_LENGTH ];

	    amount *= 37;
	    sprintf( haul, "%d", amount );
	    act( "It's a TRIPLE!  You win $t gold coins!",
		ch, haul, NULL, TO_CHAR );
	    ch->gold += amount;
	}
	else
	    send_to_char( "It's a TRIPLE!  You lose!\r\n", ch );

	return;
    }

    sprintf( buf, ", totalling %d.", total );
    strcat( msg, buf );
    act( msg, croupier, NULL, ch, TO_VICT );

    if (   ( ( total <= 10 ) && ( ichoice == 1 ) )
	|| ( ( total >= 11 ) && ( ichoice == 2 ) ) )
    {
	char haul [ MAX_STRING_LENGTH ];

	amount *= 2;
	sprintf( haul, "%d", amount );
	act( "You win $t gold coins!", ch, haul, NULL, TO_CHAR );
	ch->gold += amount;
    }
    else
	send_to_char( "Sorry, better luck next time!\r\n", ch );

    return;
}

