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
/* look up, player have a view at 40 degree (PI / 2) */
#define START_RADIANS M_PI_2
#define RAD_TURN_VAL (M_PI_4 / 8)
#define VIEW_DEEP 5
#define CASE_SUB_CASE 1000
#define CASE_HEIGHT 40

double pj_rad = START_RADIANS;

/* x = x0 + r*cos(t) */
/* y = y0 + r*sin(t) */

/* ou (x0,y0) sont les coord du centre, r est le rayon, et t l'angle. */

static void print_walls(Entity *rc);

int map_w = 8;
int map_h = 9;

uint32_t map[] = {
	-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1, 0,-1,-1,-1,
	-1,-1,-1, 0, 0,-1,-1,-1,
	-1,-1,-1,-1, 0,-1,-1,-1,
	-1,-1,-1, 0, 0, 0, 0,-1,
	-1,-1,-1, 0,-1,-1, 0,-1,
	-1, 0, 0, 0,-1,-1, 0,-1,
	-1,-1,-1,-1, 0, 0, 0,-1,
	-1,-1,-1,-1,-1,-1,-1,-1
};

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
	int x = yuiTurnX(xadd, yadd, pj_rad - M_PI_2);
	int y = yuiTurnY(xadd, yadd, pj_rad - M_PI_2);

	if (!get_case((pcx(rc)  + x) / 1000, pcy(rc) / 1000))
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

	if (yevIsKeyDown(events, 'q')) {
		pj_rad -= RAD_TURN_VAL;
		action = 1;
	} else if (yevIsKeyDown(events, 'e')) {
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
	return action == 1 ? (void *)ACTION : 0;
}

static void col_checker(int px, int py, int tpx, int tpy,
			int case_x, int case_y, int *ot, int *wall_dist)
{
	int t;
	if (!get_case(case_x, case_y))
		return;

	int r = yuiLinesRectIntersect(px, py, tpx, tpy, case_x * 1000,
				      case_y * 1000, 999, 999, 0, 0, &t);

	if (r && r < *wall_dist) {
		*wall_dist = r;
		*ot = t;
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
	Entity *walls = yeGet(rc, "walls");
	/* hiw many rad per pixiel */
	double rad_tick = (M_PI_4) / wid_w;
	double cur_rad = pj_rad - (M_PI_4 / 2);

	// range: -250, -750; 0, -1000; 250, 750
	/* printf("PY: %d\n", py); */
	/* printf("swd: %d\n", strait_wall_dist); */
	ywCanvasClear(rc);
	for (int i = 0; i < wid_w; ++i) {
		int wall_dist = py - pyc * 1000;
		int tpx = px;
		int tpy = py;
		int x_dir = 500 * cos(cur_rad) * -1;
		int y_dir = 500 * sin(cur_rad) * -1;
		/* int x_dist = px - pxc * 1000; */
		/* int y_dist = py - pyc * 1000; */

		/* printf("%d %d %f\n", x_dir, y_dir, rad_tick); */
		for (;tpy > 0 && tpx > 0 &&
			     tpx < map_w * 1000 &&
			     tpy < map_h * 1000;
		     tpy += y_dir, tpx += x_dir) {
			int case_x = tpx / 1000;
			int case_y = tpy / 1000;

			if (!get_case(case_x, case_y))
				continue;
			YE_NEW(Array, gc);
			int it;
			int it2;
			int col_x = 0;
			int col_y = 0;

			wall_dist = yuiLinesRectIntersect(px, py, tpx, tpy,
							  case_x * 1000,
							  case_y * 1000,
							  999, 999,
							  &col_x, &col_y, &it);
			case_y++;

			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);
			case_x++;
			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);
			case_x-=2;
			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);
			case_y -= 2;

			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);
			case_x++;
			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);
			case_x++;
			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);
			case_y++;
			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);
			case_x -= 2;
			col_checker(px, py, tpx, tpy, case_x, case_y,
				    &it, &wall_dist);

			/* case_x++; */
			/* case_y++; */

			/* printf("%d - %d(%d,%d,%s): - %d %d - %d %d - %d %d\n", */
			/*        i, wall_dist, */
			/*        case_x, case_y, */
			/*        yLineRectIntersectStr[it], */
			/*        px, py, tpx, tpy, */
			/*        col_x, col_y); */
			break;
		}
		int threshold = 40 * wall_dist / 1000;

		front_wall = ywCanvasNewRectangle(rc, i, threshold, 1,
						  wid_h - (threshold * 2),
						  "rgba: 20 20 20 255");
		yePushAt(walls, front_wall, i);
		cur_rad += rad_tick;
	}
	/* exit(); */
}

void *rc_init(int nbArgs, void **args)
{
	Entity *rc = args[0];
	/* double r = START_RADIANS */
	/* int x = ox * cos(r) - oy * sin(r); */
	/* int y = oy * cos(r) + ox * sin(r); */


	ywSetTurnLengthOverwrite(-1);
	printf("hey you !\n");
	YEntityBlock {
		rc.action = rc_action;
		rc.px = START_X;
		rc.py = START_Y;
		rc.walls = {};
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
		mod["window size"] = [800, 600];
		mod.test_rc["<type>"] = "raycasting";
		mod.test_rc.background = "rgba: 10 150 255 255";
		mod["window name"] = "RC";
	}

	ywidAddSubType(init);
	printf("Gretting and Salutation !\n");
	return mod;
}
