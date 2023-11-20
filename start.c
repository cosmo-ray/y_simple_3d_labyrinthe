/**        DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *                   Version 2, December 2004
 *
 * Copyright (C) 2020 Matthias Gatto <uso.cosmo.ray@gmail.com>
 *
 * Everyone is permitted to copy and distribute verbatim or modified
 * copies of this license document, and changing it is allowed as long
 * as the name is changed.
 *
 *            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *  TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *
 *  0. You just DO WHAT THE FUCK YOU WANT TO.
 */

#include <yirl/all.h>

#include "sinple_sprite.c"

#define START_X 4500
#define START_Y 4500
/* look up, player have a view at 40 degree */
/* #define FIELD_OF_VIEW 0.4981 */
#define FIELD_OF_VIEW 0.6981
/* #define FIELD_OF_VIEW M_PI_2 */
/* #define FIELD_OF_VIEW 1.0471 */
#define START_RADIANS M_PI_2
#define RAD_TURN_VAL (M_PI_4 / 8)

static double pj_rad = START_RADIANS;

static void print_walls(Entity *rc, int);

static int map_w;
static int map_h;

static int atk_rnd;

static struct {
	int type;
	int x;
	void *ptr;
} print_stack[128];
int print_stack_l;

int old_t;
int atk_cooldown;

#define EXIT_X 0
#define EXIT_Y 1
#define EXIT_DIR 2
#define EXIT_NAME 3
#define EXIT_IDX 3
#define EXIT_MARK 4

#define EXIT_SIZE (EXIT_MARK + 1)

#define MAP_MAX_SIZE 2048

#define ENEMY_IDX_POS 0
#define ENEMY_IDX_NAME 1
#define ENEMY_IDX_STATS 2
#define ENEMY_IDX_ATK_TIMER 3

#define ENEMY_ATK_COOLDOWN 800000

enum {
	DIR_DOWN,
	DIR_UP,
	DIR_LEFT,
	DIR_RIGHT,
	END_DIR
};

static char *dir_str[] = {
	"down",
	"up",
	"left",
	"right"
};

static double dir_rad[] = {
	(3 * M_PI) / 2, M_PI_2, M_PI, 0
};

static int dir_x_threshod[] = {
	0, 0, -100, 100
};

static int dir_y_threshod[] = {
	100, -100, 0, 0
};

uint32_t map[MAP_MAX_SIZE];
static int exits[MAP_MAX_SIZE][EXIT_SIZE];

unsigned int messages_idx;
char messages[0xf + 1][255];

char ux_txt[255];

#define FLAG_WALL 1
#define FLAG_EXIT 2

int attack_reach = 1100;

#define printf_msg(fmt, args...) do {			\
	int next_idx = (messages_idx - 1) & 0xf;	\
							\
	snprintf(messages[next_idx], 255, fmt, args);	\
	messages_idx = next_idx;			\
	} while(0)					\

static inline int case_idx(int x, int y)
{
	return x +  (map_w * y);
}

static inline void case_set_elem(int x, int y, int flag)
{
	map[case_idx(x, y)] |= flag;
}

static inline int get_case(int x, int y)
{
	return map[case_idx(x, y)];
}

static int pcx(Entity *rc)
{
	return yeGetIntAt(rc, "px");
}

static int pcy(Entity *rc)
{
	return yeGetIntAt(rc, "py");
}

static void mvpj(Entity *rc, int xadd, int yadd)
{
	int x = yuiTurnX(0, yadd, pj_rad - M_PI_2) -
		yuiTurnX(0, xadd, pj_rad);
	int y = yuiTurnY(0, yadd, pj_rad - M_PI_2) -
		yuiTurnY(0, xadd, pj_rad);

	if (! (get_case((pcx(rc) + x) / 1000, pcy(rc) / 1000) & 1))
		yeAddAt(rc, "px", x);
	if (! (get_case(pcx(rc) / 1000, (pcy(rc) + y) / 1000) & 1))
		yeAddAt(rc, "py", y);
}

void *rc_action(int nbArgs, void **args)
{
	Entity *rc = args[0];
	Entity *events = args[1];
	int action = 0;
	int action_keys = 0;
	int xadd = 0, yadd = 0;

	if (yevIsKeyDown(events, Y_ESC_KEY)) {
		Entity *quit_action = yeGet(rc, "quit");

		if (quit_action)
			yesCall(quit_action, rc);
		else
			ygTerminate();

		return (void *)ACTION;
	}

	if (yevIsKeyDown(events, '\n') ||
	    yevIsKeyDown(events, ' ')) {
		printf("ACTION KEY ENTER !!!!");
		action_keys = 1;
	} else if (yevIsKeyDown(events, 'p')) {
		printf("ACTION KEY ENTER !!!!");
		action_keys = 2;
	}
	if (yevIsKeyDown(events, 'q') ||
	    yevIsKeyDown(events, 'a')) {
		pj_rad -= RAD_TURN_VAL;
		action = 1;
	} else if (yevIsKeyDown(events, 'e') ||
		   yevIsKeyDown(events, 'd')) {
		pj_rad += RAD_TURN_VAL;
		action = 1;
	}
	/* trigo circle is between M_PI and -M_PI */
	if (pj_rad > M_PI) {
		pj_rad -= 2 * M_PI;
	} else if (pj_rad < -M_PI) {
		pj_rad += 2 * M_PI;
	}

	if (yevIsKeyDown(events, Y_UP_KEY) || yevIsKeyDown(events, 'w')) {
		yadd = -100;
		action = 1;
	} else if (yevIsKeyDown(events, Y_DOWN_KEY) || yevIsKeyDown(events, 's')) {
		yadd = 100;
		action = 1;
	}
	if (yevIsKeyDown(events, Y_LEFT_KEY)) {
		xadd = -100;
		action = 1;
	} else if (yevIsKeyDown(events, Y_RIGHT_KEY)) {
		xadd = 100;
		action = 1;
	}
	print_walls(rc, action_keys);
	if (action)
		mvpj(rc, xadd, yadd);
	return (void *)ACTION;
}

static int col_checker(int px, int py, int tpx, int tpy,
			int case_x, int case_y, int *ot, int *wall_dist)
{
	int t;
	int x, y;
	uint32_t f = get_case(case_x, case_y);
	int ret = 0;

	if (!(f & FLAG_WALL)) {
		if (f & FLAG_EXIT) {
			int etx = exits[case_idx(case_x, case_y)][EXIT_X];
			int ety = exits[case_idx(case_x, case_y)][EXIT_Y];

			int r = yuiLinesRectIntersect(px, py, tpx, tpy, etx, ety,
						      10, 10, &x, &y, &t);
			if (r && !exits[case_idx(case_x, case_y)][EXIT_MARK]) {
				exits[case_idx(case_x, case_y)][EXIT_MARK] = 1;
				print_stack[print_stack_l].type = FLAG_EXIT;
				print_stack[print_stack_l].ptr = &exits[case_idx(case_x, case_y)];
				++print_stack_l;
				++ret;
			}
		}

		// no colision
		return ret;
	}

	int r = yuiLinesRectIntersect(px, py, tpx, tpy, case_x * 1000 - 1,
				      case_y * 1000 - 1, 1001, 1001, &x, &y, &t);

	if (r) {
		/* r = yuiPointsDist(px, py, px + (x - px) / 2, y); */
		if (!(*wall_dist) || r < *wall_dist) {
			*wall_dist = r;
			*ot = t;
		}
	}
	return ret;
}

static void print_walls(Entity *rc, int action_keys)
{
	Entity *front_wall;
	Entity *ofw;
	Entity *enemies = yeGet(rc, "enemies");
	int wid_w = ywRectW(yeGet(rc, "wid-pix"));
	int wid_h = ywRectH(yeGet(rc, "wid-pix"));
	int px = yeGetIntAt(rc, "px");
	int py = yeGetIntAt(rc, "py");
	int pyc = py / 1000;
	int pxc = px / 1000;
	yeAutoFree Entity *pc_pos = ywPosCreateInts(px, py, NULL, NULL);
	/* hiw many rad per pixiel */
	double r0 = pj_rad - (FIELD_OF_VIEW / 2);
	double cur_rad = r0;
	Entity *target_enemy = NULL;
	int turn_timer = ywidGetTurnTimer();
	Entity *pc_e = yeGet(rc, "pc");
	int pc_esquive = yeGetIntAt(yeGet(pc_e, "stats"), "agility") * 2;

	printf("%d - %d\n", wid_w, wid_h);
	ywCanvasMergeRectangle(rc, 0, 0, wid_w, wid_h / 2,
			       "rgba: 150 150 150 255");
	ywCanvasMergeRectangle(rc, 0, wid_h / 2, wid_w, wid_h / 2,
			       "rgba: 100 150 255 255");

	/* For each rays */
	for (int i = 0; i < wid_w; ++i) {
		int wall_dist = 0;
		int tpx = px;
		int tpy = py;
		int x_dir = 1000 * cos(cur_rad) * -1;
		int y_dir = 1000 * sin(cur_rad) * -1;

		for (;tpy > 0 && tpx > 0 &&
			     tpx < map_w * 1000 &&
			     tpy < map_h * 1000;
		     tpy += y_dir, tpx += x_dir) {
			int case_x = tpx / 1000;
			int case_y = tpy / 1000;
			int printable = 0;
			int it;

			/* moiddle case */
			printable += col_checker(px, py, tpx, tpy, case_x, case_y,
						 &it, &wall_dist);
			case_y++;

			/* top cases */
			printable += col_checker(px, py, tpx, tpy, case_x, case_y,
						 &it, &wall_dist);

			/* bottom cases */
			case_y -= 2;
			printable += col_checker(px, py, tpx, tpy, case_x, case_y,
						 &it, &wall_dist);

			/* middle again */
			case_y++;
			case_x++;
			printable += col_checker(px, py, tpx, tpy, case_x, case_y,
						 &it, &wall_dist);
			case_x -= 2;
			printable +=  col_checker(px, py, tpx, tpy, case_x, case_y,
						  &it, &wall_dist);

			for (;printable; --printable) {
				print_stack[print_stack_l - printable].x = i;
			}

			if (wall_dist) {
				break;
			}
		}
		wall_dist *= cos(cur_rad - pj_rad);
		double wall_h = wid_h * wid_h / (wall_dist / 1.4);
		ywCanvasMergeRectangle(rc, i, wid_h < wall_h ? 0 :
				       (wid_h - wall_h) / 2 , 1,
				       wall_h,
				       "rgba: 220 20 10 255");

		cur_rad = r0 + FIELD_OF_VIEW * i / wid_h;
	}

	int pj_angle = pj_rad * (180 / M_PI) + 180;
	pj_angle = pj_angle % 360;

	YE_FOREACH(enemies, e) {
		Entity *e_pos = yeGet(e, ENEMY_IDX_POS);
		Entity *e_atk_timer = yeGet(e, ENEMY_IDX_ATK_TIMER);
		const char *e_name = yeGetStringAt(e, ENEMY_IDX_NAME);
		void *enemy = e_name[0] == 'r' ? &rat : &bguy;
		int max_size = 500;
		int min_size = 10;
		int dist = ywPosDistance(pc_pos, e_pos);
		Entity *stats = yeGet(e, ENEMY_IDX_STATS);
		int life = yeGetIntAt(stats, 0);
		if (dist > 7000)
			continue;
		yeAdd(e_atk_timer, -turn_timer);
		if (life < 3) {
			if (enemy == &rat)
				enemy = &rat_hurt_2;
			else if (enemy == &bguy)
				enemy = &bguy_hurt_2;
		} else if (life < 8) {
			if (enemy == &rat)
				enemy = &rat_hurt_1;
			else if (enemy == &bguy)
				enemy = &bguy_hurt_1;
		}

		if (dist < attack_reach && yeGetIntAt(e, ENEMY_IDX_ATK_TIMER) <= 0) {
			int max_atk = yeGetIntAt(yeGet(stats, "stats"), "strength");
			int dmg = yuiRand() % max_atk + 1;
			int miss = (yuiRand() % 100) < pc_esquive;
			yeSetAt(e, ENEMY_IDX_ATK_TIMER, ENEMY_ATK_COOLDOWN);
			printf("stats:\n");
			yePrint(stats);
			if (miss)
				printf_msg("%s miss", e_name, max_atk, dmg);
			else {
				yeAddAt(pc_e, "life", -dmg);
				printf_msg("%s atk you for 1-%d, deal: %d", e_name, max_atk, dmg);
				if (yeGetIntAt(pc_e, "life") <= 0) {
					Entity *die_action = yeGet(rc, "die");

					if (die_action)
						yesCall(die_action, rc);
					else
						ygTerminate();
				}
			}
		}

		int size_xy = 500.0 - 100.0 * (dist / 1000.0);
		/* printf("size_xy: %f\n", size_xy); */

		int relative_angle = pj_angle - (ywPosAngle(pc_pos, e_pos) + 180);
		relative_angle = relative_angle % 360;
		if (relative_angle > 180)
			relative_angle -= 360;
		else if (relative_angle < -180)
			relative_angle += 360;
		printf("enemy dist %d enemy-pc angle: %f pc rad: %f pc andle: %d: relative: %d\n",
		       dist, ywPosAngle(pc_pos, e_pos) + 180,
		       pj_rad, pj_angle,
		       relative_angle);

		int enemy_x = (wid_w) - ((relative_angle + 45) * (size_xy + wid_w) / 90) - size_xy / 2;
		if (abs(relative_angle) > 45) {
			continue;
		}
		yeAutoFree Entity *e_pcopy = yeCreateCopy(e_pos, NULL, NULL);
		int skipp = 0;
		int x_dist = abs(ywPosXDistance(pc_pos, e_pos));
		int y_dist = abs(ywPosYDistance(pc_pos, e_pos));
		int advance_x = 1 + x_dist * 20 / dist;
		int advance_y = 1 + y_dist * 20 / dist;

		while (ywPosMoveToward2(e_pcopy, pc_pos, advance_x, advance_y)) {
			if (get_case(ywPosX(e_pcopy) / 1000, ywPosY(e_pcopy) / 1000) & FLAG_WALL) {
				skipp = 1;
				break;
			}
		}

		if (skipp)
			continue;

		/* wall colision here */
		Entity *r = y_ssprite_obj(rc, enemy,
					  enemy_x,
					  // compute botom pos depending on distance,
					  // the futher the less high
					  300
					  + (20.0 * (dist / 1000.0))
					  + (dist > 3000 ? (30.0 * (dist / 1000.0)) - 90 : 0)
					  + (dist > 4000 ? 3 * ((dist - 4000) / 100) : 0)
			);
		yeAutoFree Entity *size = ywSizeCreate(size_xy, size_xy, NULL, NULL);
		ywCanvasForceSize(r, size);
		ywCanvasMergeObj(rc, r);
		if (dist < attack_reach && abs(relative_angle) < 25) {
			target_enemy = e;
		}
	}


	for (int i = 0; i < print_stack_l; ++i) {
		int x = print_stack[i].x;
		int (*e)[EXIT_SIZE] = print_stack[i].ptr;
		int ex = (*e)[EXIT_X];
		int ey = (*e)[EXIT_Y];
		int ed = yuiPointsDist(px, py, ex, ey);
		double e_h = ed < 30 ? wid_h : wid_h * wid_h / (ed / 1.4);
		double e_w = ed < 30 ? 220 : 220 * 220 / (ed / 1.7);

		if (action_keys == 1 && abs((ex + ey) - (px + py)) < 800) {
			Entity *exit_action = yeGet(rc, "exit_action");
			int idx = (*e)[EXIT_IDX];

			/* printf("ACTION ON EXIT %p !!!!\n", yeGet(rc, "exits")); */
			/* printf("ACTION ON EXIT %s %d !!!!\n", */
			/*        yeGetStringAt(yeGet(yeGet(rc, "exits"), idx), */
			/* 		     EXIT_NAME + 1), */
			/*        yeGetIntAt(yeGet(yeGet(rc, "exits"), idx), */
			/* 		  EXIT_NAME + 2)); */
			if (exit_action) {
				yesCall(exit_action, rc, NULL,
					yeGetStringAt(yeGet(yeGet(rc, "exits"), idx),
						      EXIT_NAME + 1),
					yeGetIntAt(yeGet(yeGet(rc, "exits"), idx),
						   EXIT_NAME + 2));
			} else {
				ygTerminate();
			}
			goto out;
		}
		/* printf("ELEM %d IN PRINT STACK\n", i); */
		/* printf("%d %d %d %d\n", (*e)[0], (*e)[1], (*e)[2], (*e)[3]); */
		/* printf("dist: %d %f\n", yuiPointsDist(px, py, ex, ey), e_h); */
		/* ywCanvasMergeRectangle(rc, x, */
		/* 		       wid_h < e_h ? 0 : */
		/* 		       (wid_h - e_h) / 2, // x, y */
		/* 		       e_w, e_h, */
		/* 		       "rgba: 127 127 127 255"); */
		Entity *lad = y_ssprite_obj(rc, &ladder, x, wid_h < e_h ? 0 :
					    (wid_h - e_h) / 2);
		yeAutoFree Entity *size = ywSizeCreate(e_w, e_h,
						       NULL, NULL);
		ywCanvasForceSize(lad, size);
		ywCanvasMergeObj(rc, lad);

		(*e)[EXIT_MARK] = 0;
	}
	if ((action_keys && atk_cooldown <= 0) || atk_cooldown > 300000) {
		if (action_keys && atk_cooldown <= 0) {
			Entity *enemy_stat = yeGet(target_enemy, ENEMY_IDX_STATS);
			atk_cooldown = 600000;
			atk_rnd = yuiRand();
			printf_msg("PUNCH on %s !",
				   target_enemy ?
				   yeGetStringAt(target_enemy, ENEMY_IDX_NAME) :
				   "Nothing");
			yeAddAt(enemy_stat, "life", -4);
			if (yeGetIntAt(enemy_stat, "life") < 1) {
				yeRemoveChild(enemies, target_enemy);
			}
		}

		Entity *p = y_ssprite_obj(rc, atk_rnd & 1 ? &punch_r : &punch,
					  atk_rnd & 1 ? 600 : 250,
					  wid_h - 150 - 350);
		yeAutoFree Entity *size = ywSizeCreate(350, 350,
						       NULL, NULL);
		ywCanvasForceSize(p, size);
	}
	atk_cooldown -= turn_timer;

out:

	/* Print UX, could be in another function */

	/* characters info UX */
	{
		const char *name = yeGetStringAt(pc_e, "name");
		ywCanvasMergeRectangle(rc, 0, 0, 110, wid_h - 150,
				       "rgba: 100 100 100 50");

		int y_txt = 5;
		ywCanvasMergeText(rc, y_txt, 0, 70, 30,
				  name ? name : "<CHAR NAME>");
		y_txt += 20;

		snprintf(ux_txt, 255, "HP: %d/%d", yeGetIntAt(pc_e, "life"),
			yeGetIntAt(pc_e, "max_life"));
		ux_txt[254] = 0;
		ywCanvasMergeText(rc, 10, y_txt, 70, 30, ux_txt);
		y_txt += 20;

		snprintf(ux_txt, 255, "Can ATK: %s", atk_cooldown <= 0 ? "OK" : "NO");
		ux_txt[254] = 0;
		ywCanvasMergeText(rc, 10, y_txt, 70, 30, ux_txt);
		y_txt += 20;

		snprintf(ux_txt, 255, "DMG: 1-%d", 1 + yeGetIntAt(yeGet(pc_e, "stats"), "strength"));
		ux_txt[254] = 0;
		ywCanvasMergeText(rc, 10, y_txt, 70, 30, ux_txt);
		y_txt += 20;

		snprintf(ux_txt, 255, "Esquive: %d%%", pc_esquive);
		ux_txt[254] = 0;
		ywCanvasMergeText(rc, 10, y_txt, 70, 30, ux_txt);
		y_txt += 20;
		y_txt += 10;

		ywCanvasMergeRectangle(rc, 0, y_txt, 110, 5,
				       "rgba: 200 200 200 200");

	}


	/* Game message UX */
	ywCanvasMergeRectangle(rc, 0, wid_h - 150,
			       wid_w, 150,
			       "rgba: 200 200 200 100");

	for (int i = 0; i < 8; ++i) {
		ywCanvasMergeText(rc, 5, wid_h - 140 + (i * 18), wid_h - 20, 20, messages[(messages_idx + i ) & 0xf]);
	}

	print_stack_l = 0;
}

#define FAIL(fmt...) do {				\
		DPRINT_ERR(fmt);			\
		return NULL;				\
	} while (0);


void rc_destroy()
{
	y_ssprites_deinit();
	ywSetTurnLengthOverwrite(old_t);
}

void *rc_init(int nbArgs, void **args)
{
	Entity *rc = args[0];
	Entity *map_e = yeGet(rc, "map");
	uint32_t *map_p = map;
	Entity *exits_e = yeGet(rc, "exits");
	int start_x = START_X, start_y = START_Y;
	const char *in = yeGetStringAt(rc, "entry");
	int nb_exits;
	Entity *enemies = yeGet(rc, "enemies");

	if (!map_e)
		FAIL("no map");

	map_h = yeLen(map_e);
	if (!map_h)
		FAIL("incorect map");

	map_w = yeLenAt(map_e, 0);

	if (!map_w)
		FAIL("incorect map W");

	if (map_w * map_h > MAP_MAX_SIZE)
		FAIL("map too big");
	
	for (int i = 0; i < map_h; ++i) {
		const char *line = yeGetStringAt(map_e, i);

		if (yeLenAt(map_e, i) != map_w)
			FAIL("non equal map W");
		for (const char *tmp = line; *tmp; ++tmp) {
			*map_p++ = *tmp == '.' ? 0 : FLAG_WALL;
		}
	}

	nb_exits = yeLen(exits_e);
	for (int i = 0; i < nb_exits; ++i) {
		Entity *e = yeGet(exits_e, i);
		const char *sdir = yeGetStringAt(e, EXIT_DIR);
		const char *name = yeGetStringAt(e, EXIT_NAME);
		int x = yeGetIntAt(e, EXIT_X);
		int y = yeGetIntAt(e, EXIT_Y);
		int idx = case_idx(x / 1000, y / 1000);

		// check out of bounds would be nice here.
		exits[idx][EXIT_X] = x;
		exits[idx][EXIT_Y] = y;
		exits[idx][EXIT_IDX] = i;
		if (sdir)
			for (int j = 0;  j < END_DIR; ++j) {
				if (!strcmp(sdir, dir_str[j])) {
					exits[idx][EXIT_DIR] = j;
				}
			}
		else
			exits[i][EXIT_DIR] = 0;

		if (yuiStrEqual0(name, in)) {
			start_x = exits[idx][EXIT_X] + dir_x_threshod[exits[idx][EXIT_DIR]];
			start_y = exits[idx][EXIT_Y] + dir_y_threshod[exits[idx][EXIT_DIR]];
			pj_rad = dir_rad[exits[idx][EXIT_DIR]];
		}
		case_set_elem(x / 1000, y / 1000, FLAG_EXIT);
	}

	old_t = ywGetTurnLengthOverwrite();
	ywSetTurnLengthOverwrite(-1);
	YEntityBlock {
		rc.action = rc_action;
		rc.destroy = rc_destroy;
		rc.px = start_x;
		rc.py = start_y;
		rc.mergable = 1;
	}

	YE_FOREACH(enemies, e) {
		Entity *e_stats = yeGet(e, ENEMY_IDX_STATS);
		const char *name = yeGetStringAt(e, ENEMY_IDX_NAME);
		int max_life = 10;
		if (!e_stats)
			e_stats = yeCreateArray(e, e_stats);
		if (yeType(e_stats) != YARRAY) {
			DPRINT_ERR("enemy stats not an array !\n");
			return NULL;
		}
		//yeTryCreateInt(max_life, e_stats, "max_life");
		yeReCreateInt(max_life, e_stats, "life");
		Entity *stats = yeTryCreateArray(e_stats, "stats");
		yeTryCreateInt(2, stats, "strength");
		yeTryCreateInt(1, stats, "agility");
		yeCreateIntAt(0, e, "atk_timer", ENEMY_IDX_ATK_TIMER);
	}

	y_ssprites_init();
	void *ret = ywidNewWidget(rc, "canvas");
	print_walls(rc, 0);
	return ret;
}

void *mod_init(int nbArg, void **args)
{
	Entity *mod = args[0];
	Entity *init = yeCreateArray(NULL, NULL);

	YEntityBlock {
		init.name = "raycasting";
		init.callback = rc_init;
		mod.name = "raycasting";
		mod.starting_widget = "test_rc";
		mod.test_rc = [];
		mod["window size"] = [1300, 800];
		mod.test_rc["<type>"] = "raycasting";
		mod.test_rc.background = "rgba: 10 150 255 255";
		mod.test_rc.map = [
			"########",
			"#.....##",
			"#.#...##",
			"#.#...##",
			"#.#....#",
			"###.##.#",
			"#...##.#",
			"####...#",
			"########"
			];
		mod.test_rc.pc = {};
		mod.test_rc.pc.life = 10;
		mod.test_rc.pc.max_life = 10;
		mod.test_rc.pc.stats = {};
		mod.test_rc.pc.stats.strength = 3;
		mod.test_rc.pc.stats.agility = 2;
		mod.test_rc.exits = {};
		mod.test_rc.exits[0] = [1200, 1700];
		mod.test_rc.enemies = {};
		mod.test_rc.enemies[0] = [[4200, 1700], "rat"];
		mod.test_rc.enemies[1] = [[1200, 3700], "bguy"];
		mod["window name"] = "RC";
	}

	ywidAddSubType(init);
	printf("Gretting and Salutation !\n");
	return mod;
}
