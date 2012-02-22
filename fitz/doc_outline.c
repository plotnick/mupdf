#include "fitz.h"

void
fz_free_outline(fz_context *ctx, fz_outline *outline)
{
	while (outline)
	{
		fz_outline *next = outline->next;
		fz_free_outline(ctx, outline->down);
		fz_free(ctx, outline->title);
		fz_free_link_dest(ctx, &outline->dest);
		fz_free(ctx, outline);
		outline = next;
	}
}

void
fz_debug_outline_xml(fz_context *ctx, fz_outline *outline, int level)
{
	while (outline)
	{
		printf("<outline title=\"%s\" page=\"%d\"", outline->title, outline->dest.kind == FZ_LINK_GOTO ? outline->dest.ld.gotor.page + 1 : 0);
		if (outline->down)
		{
			printf(">\n");
			fz_debug_outline_xml(ctx, outline->down, level + 1);
			printf("</outline>\n");
		}
		else
		{
			printf(" />\n");
		}
		outline = outline->next;
	}
}

void
fz_debug_outline(fz_context *ctx, fz_outline *outline, int level)
{
	int i;
	while (outline)
	{
		for (i = 0; i < level; i++)
			putchar('\t');
		printf("%s\t%d\n", outline->title, outline->dest.kind == FZ_LINK_GOTO ? outline->dest.ld.gotor.page + 1 : 0);
		if (outline->down)
			fz_debug_outline(ctx, outline->down, level + 1);
		outline = outline->next;
	}
}
