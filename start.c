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

#include <yirl/entity.h>
#include <yirl/events.h>
#include <yirl/entity-script.h>
#include <yirl/canvas.h>
#include <yirl/game.h>
#include <yirl/timer.h>

#define START_X 4500
#define START_Y 4500
/* look up, player have a view at 40 degree */
/* #define FIELD_OF_VIEW 0.4981 */
#define FIELD_OF_VIEW 0.6981
/* #define FIELD_OF_VIEW M_PI_2 */
/* #define FIELD_OF_VIEW 1.0471 */
#define START_RADIANS M_PI_2
#define RAD_TURN_VAL (M_PI_4 / 8)

double pj_rad = START_RADIANS;

static void print_walls(Entity *rc);

int map_w;
int map_h;

#define MAP_MAX_SIZE 2048

uint32_t map[MAP_MAX_SIZE];

static int get_case(int x, int y)
{
	return map[x +  (map_w * y)];
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

	if (!get_case((pcx(rc) + x) / 1000, pcy(rc) / 1000))
		yeAddAt(rc, "px", x);
	if (!get_case(pcx(rc) / 1000, (pcy(rc) + y) / 1000))
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

static void col_checker(int px, int py, int tpx, int tpy,
			int case_x, int case_y, int *ot, int *wall_dist)
{
	int t;
	int x, y;

	if (!get_case(case_x, case_y))
		return;

	int r = yuiLinesRectIntersect(px, py, tpx, tpy, case_x * 1000 - 1,
				      case_y * 1000 - 1, 1001, 1001, &x, &y, &t);

	if (r) {
		/* r = yuiPointsDist(px, py, px + (x - px) / 2, y); */
		if (!(*wall_dist) || r < *wall_dist) {
			*wall_dist = r;
			*ot = t;
		}
	}
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
	ywCanvasMergeRectangle(rc, 0, 0, wid_w, wid_h,
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
			int it;

			/* moddie case */
			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);
			case_y++;

			/* top cases */
			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);

			/* bottom cases */
			case_y -= 2;
			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);

			/* middle again */
			case_y++;
			case_x++;
			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);
			case_x -= 2;
			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);

			if (wall_dist) {
				break;
			}
		}
		wall_dist *= cos(cur_rad - pj_rad);
		double wall_h = wid_h * wid_h / (wall_dist / 1.4);
		ywCanvasMergeRectangle(rc, i, wid_h < wall_h ? 0 :
				       (wid_h - wall_h) / 2 , 1,
				       wall_h,
				       "rgba: 20 20 20 255");

		cur_rad = r0 + FIELD_OF_VIEW * i / wid_h;
	}
}

#define FAIL(fmt...) do {				\
		DPRINT_ERR(fmt);			\
		return NULL;				\
	} while (0);

void *rc_init(int nbArgs, void **args)
{
	Entity *rc = args[0];
	Entity *map_e = yeGet(rc, "map");
	uint32_t *map_p = map;

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
		char *line = yeGetStringAt(map_e, i);

		if (yeLenAt(map_e, i) != map_w)
			FAIL("non equal map W");
		for (char *tmp = line; *tmp; ++tmp) {
			*map_p++ = *tmp == '.' ? 0 : -1;
		}
	}


	ywSetTurnLengthOverwrite(-1);
	printf("hey you !\n");
	YEntityBlock {
		rc.action = rc_action;
		rc.px = START_X;
		rc.py = START_Y;
		rc.mergable = 1;
	}

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
