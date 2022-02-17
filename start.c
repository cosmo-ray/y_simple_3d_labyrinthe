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

static void print_walls(Entity *rc);

static int map_w;
static int map_h;

static struct {
	int type;
	int x;
	void *ptr;
} print_stack[128];
int print_stack_l;

#define EXIT_X 0
#define EXIT_Y 1
#define EXIT_DIR 2
#define EXIT_NAME 3
#define EXIT_MARK 3

#define EXIT_SIZE (EXIT_MARK + 1)

#define MAP_MAX_SIZE 2048

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

#define FLAG_WALL 1
#define FLAG_EXIT 2

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
	int xadd = 0, yadd = 0;

	if (yevIsKeyDown(events, Y_ESC_KEY)) {
		Entity *quit_action = yeGet(rc, "quit");

		if (quit_action)
			yesCall(quit_action, rc);
		else
			ygTerminate();

		return (void *)ACTION;
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

	if (yevIsKeyDown(events, Y_UP_KEY)) {
		yadd = -100;
		action = 1;
	} else if (yevIsKeyDown(events, Y_DOWN_KEY)) {
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
	print_walls(rc);
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
						      5, 5, &x, &y, &t);
			if (r && !exits[case_idx(case_x, case_y)][EXIT_MARK]) {
				exits[case_idx(case_x, case_y)][EXIT_MARK] = 1;
				print_stack[print_stack_l].type = FLAG_EXIT;
				print_stack[print_stack_l].ptr = &exits[case_idx(case_x, case_y)];
				++print_stack_l;
				++ret;
				printf("EXIT IN %d %d %s -- ", etx, ety,
				       dir_str[exits[case_idx(case_x, case_y)][EXIT_DIR]]);
				printf("%d %d %d %d %d %d\n", px,
				       py, tpx, tpy, case_x, case_y);
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

static void print_walls(Entity *rc)
{
	Entity *front_wall;
	Entity *ofw;
	int wid_w = ywRectW(yeGet(rc, "wid-pix"));
	int wid_h = ywRectH(yeGet(rc, "wid-pix"));
	int px = yeGetIntAt(rc, "px");
	int py = yeGetIntAt(rc, "py");
	int pyc = py / 1000;
	int pxc = px / 1000;
	/* hiw many rad per pixiel */
	double r0 = pj_rad - (FIELD_OF_VIEW / 2);
	double cur_rad = r0;

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
	for (int i = 0; i < print_stack_l; ++i) {
		int x = print_stack[i].x;
		int (*e)[EXIT_SIZE] = print_stack[i].ptr;
		int ex = (*e)[EXIT_X];
		int ey = (*e)[EXIT_Y];
		int ed = yuiPointsDist(px, py, ex, ey);
		double e_h = ed < 30 ? wid_h : wid_h * wid_h / (ed / 1.4);
		double e_w = ed < 30 ? 220 : 220 * 220 / (ed / 1.7);

		printf("ELEM %d IN PRINT STACK\n", i);
		printf("%d %d %d %d\n", (*e)[0], (*e)[1], (*e)[2], (*e)[3]);
		printf("dist: %d %f\n", yuiPointsDist(px, py, ex, ey), e_h);
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


		(*e)[EXIT_MARK] = 0;
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

	ywSetTurnLengthOverwrite(-1);
	YEntityBlock {
		rc.action = rc_action;
		rc.destroy = rc_destroy;
		rc.px = start_x;
		rc.py = start_y;
		rc.mergable = 1;
	}

	y_ssprites_init();
	void *ret = ywidNewWidget(rc, "canvas");
	print_walls(rc);
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
		mod["window name"] = "RC";
	}

	ywidAddSubType(init);
	printf("Gretting and Salutation !\n");
	return mod;
}
