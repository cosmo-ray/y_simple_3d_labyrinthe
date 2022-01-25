struct ssprite {
	char *c_b;
	char *c_f;
	char *end_def;
	Entity *info;
	char *d[];
};


#define ADD_SSPRITE(name, col_b, col_f, def...)		\
	struct ssprite name = {				\
		col_b, col_f, NULL, NULL, {def, NULL}	\
	};

#include "sprites.h"

#define ADD_SSPRITE(name, col_b, col_f, def...)	\
	&name ,

struct ssprite *all_ssprites[] = {
#include "sprites.h"
	NULL
};

void y_ssprites_init(void)
{
	for (struct ssprite **pp = all_ssprites; *pp; ++pp) {
		int l = 0, i = 0;
		struct ssprite *p = *pp;
		int w = p->d[0] ? strlen(p->d[0]) : 0;

		for (char **tmp = p->d; *tmp; ++tmp) {
			l += w;
		}
		if (!l)
			return;

		p->end_def = malloc(l + 1);
		p->end_def[l] = 0;
		p->info = yeCreateArray(NULL, NULL);

		l = 0;
		for (char **tmp_l = p->d; *tmp_l; ++tmp_l) {
			for (char *tmp_w = *tmp_l; *tmp_w; ++tmp_w) {
				p->end_def[i++] = *tmp_w == ' ' ? 0 : 1;
			}
			++l;
		}
		ywSizeCreate(w, l, p->info, NULL);
		yeCreateString(p->c_b, p->info, NULL);
		yeCreateString(p->c_f, p->info, NULL);

	}
}

void y_ssprites_deinit(void)
{
	for (struct ssprite **p = all_ssprites; *p; ++p) {
		yeDestroy((*p)->info);
		free((*p)->end_def);
		(*p)->end_def = NULL;
		(*p)->info = NULL;
	}
}

Entity *y_ssprite_obj(Entity *cwid, struct ssprite *s, int x, int y)
{
	return ywCanvasNewBicolorImg(cwid, x, y, s->end_def, s->info);
}
