#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "cmark.h"
#include "node.h"
#include "buffer.h"
#include "houdini.h"

// Functions to convert cmark_nodes to HTML strings.

static void escape_html(cmark_strbuf *dest, const unsigned char *source, int length)
{
	if (length < 0)
		length = strlen((char *)source);

	houdini_escape_html0(dest, source, (size_t)length, 0);
}

static void escape_href(cmark_strbuf *dest, const unsigned char *source, int length)
{
	if (length < 0)
		length = strlen((char *)source);

	houdini_escape_href(dest, source, (size_t)length);
}

static inline void cr(cmark_strbuf *html)
{
	if (html->size && html->ptr[html->size - 1] != '\n')
		cmark_strbuf_putc(html, '\n');
}

struct render_state {
	cmark_strbuf* html;
	cmark_node *plain;
};

static int
S_render_node(cmark_node *node, cmark_event_type ev_type, void *vstate)
{
	struct render_state *state = vstate;
	cmark_node *parent;
	cmark_node *grandparent;
	cmark_strbuf *html = state->html;
	char start_header[] = "#######";
	char end_header[] = " #######";
	bool tight;

	bool entering = (ev_type == CMARK_EVENT_ENTER);

	if (state->plain == node) { // back at original node
		state->plain = NULL;
	}

	if (state->plain != NULL) {
		switch(node->type) {
		case CMARK_NODE_TEXT:
		case CMARK_NODE_CODE:
		case CMARK_NODE_INLINE_HTML:
			escape_html(html, node->as.literal.data,
				    node->as.literal.len);
			break;

		case CMARK_NODE_LINEBREAK:
		case CMARK_NODE_SOFTBREAK:
			cmark_strbuf_putc(html, ' ');
			break;

		default:
			break;
		}
		return 1;
	}

	switch (node->type) {
	case CMARK_NODE_DOCUMENT:
		break;

	case CMARK_NODE_BLOCK_QUOTE:
		if (entering) {
			cr(html);
			cmark_strbuf_puts(html, "<blockquote>\n");
		} else {
			cr(html);
			cmark_strbuf_puts(html, "</blockquote>\n");
		}
		break;

	case CMARK_NODE_LIST: {
		cmark_list_type list_type = node->as.list.list_type;
		int start = node->as.list.start;

		if (entering) {
			cr(html);
			if (list_type == CMARK_BULLET_LIST) {
				cmark_strbuf_puts(html, "<ul>\n");
			}
			else if (start == 1) {
				cmark_strbuf_puts(html, "<ol>\n");
			}
			else {
				cmark_strbuf_printf(html, "<ol start=\"%d\">\n",
					      start);
			}
		} else {
			cmark_strbuf_puts(html,
				    list_type == CMARK_BULLET_LIST ?
				    "</ul>\n" : "</ol>\n");
		}
		break;
	}

	case CMARK_NODE_ITEM:
		if (entering) {
			cr(html);
			cmark_strbuf_puts(html, "");
		} else {
			cmark_strbuf_puts(html, "</li>\n");
		}
		break;

	case CMARK_NODE_HEADER:
		if (entering) {
			cr(html);
			start_header[node->as.header.level] = '\0';
			cmark_strbuf_puts(html, start_header);
		} else {
			end_header[1 + node->as.header.level] = '\0';
			cmark_strbuf_puts(html, end_header);
			cmark_strbuf_putc(html, '\n');
		}
		break;

	case CMARK_NODE_CODE_BLOCK:
		cr(html);

		if (&node->as.code.fence_length == 0
		    || node->as.code.info.len == 0) {
			cmark_strbuf_puts(html, "<pre><code>");
		}
		else {
			int first_tag = 0;
			while (first_tag < node->as.code.info.len &&
			       node->as.code.info.data[first_tag] != ' ') {
				first_tag += 1;
			}

			cmark_strbuf_puts(html, "<pre><code class=\"language-");
			escape_html(html, node->as.code.info.data, first_tag);
			cmark_strbuf_puts(html, "\">");
		}

		escape_html(html, node->as.literal.data, node->as.literal.len);
		cmark_strbuf_puts(html, "</code></pre>\n");
		break;

	case CMARK_NODE_HTML:
		cr(html);
		cmark_strbuf_put(html, node->as.literal.data, node->as.literal.len);
		break;

	case CMARK_NODE_HRULE:
		cr(html);
		cmark_strbuf_puts(html, "------\n");
		break;

	case CMARK_NODE_PARAGRAPH:
		parent = cmark_node_parent(node);
		grandparent = cmark_node_parent(parent);
		if (grandparent != NULL &&
		    grandparent->type == CMARK_NODE_LIST) {
			tight = grandparent->as.list.tight;
		} else {
			tight = false;
		}
		if (!tight) {
			if (entering) {
				cr(html);
				cmark_strbuf_puts(html, "\n");
			} else {
				cmark_strbuf_puts(html, "\n");
			}
		}
		break;

	case CMARK_NODE_TEXT:
		escape_html(html, node->as.literal.data,
			    node->as.literal.len);
		break;

	case CMARK_NODE_LINEBREAK:
		cmark_strbuf_puts(html, "\n");
		break;

	case CMARK_NODE_SOFTBREAK:
		cmark_strbuf_putc(html, '\n');
		break;

	case CMARK_NODE_CODE:
		cmark_strbuf_puts(html, "<code>");
		escape_html(html, node->as.literal.data, node->as.literal.len);
		cmark_strbuf_puts(html, "</code>");
		break;

	case CMARK_NODE_INLINE_HTML:
		cmark_strbuf_put(html, node->as.literal.data, node->as.literal.len);
		break;

	case CMARK_NODE_STRONG:
		if (entering) {
			cmark_strbuf_puts(html, "**");
		} else {
			cmark_strbuf_puts(html, "**");
		}
		break;

	case CMARK_NODE_EMPH:
		if (entering) {
			cmark_strbuf_puts(html, "*");
		} else {
			cmark_strbuf_puts(html, "*");
		}
		break;

	case CMARK_NODE_LINK:
		if (entering) {
			cmark_strbuf_puts(html, "[");
			if (node->as.link.title) {
				escape_html(html, node->as.link.title, -1);
			}
			cmark_strbuf_puts(html, "](");
			if (node->as.link.url)
			{
				escape_href(html, node->as.link.url, -1);
			}
			cmark_strbuf_puts(html, ")");
		}
		break;

	case CMARK_NODE_IMAGE:
		if (entering) {
			cmark_strbuf_puts(html, "![");
			if (node->as.link.title) {
				escape_html(html, node->as.link.title, -1);
			}
			cmark_strbuf_puts(html, "](");
			if (node->as.link.url)
			{
				escape_href(html, node->as.link.url, -1);
			}
			cmark_strbuf_puts(html, ")");
			state->plain = node;
		}
		break;

	default:
		assert(false);
		break;
	}

	// cmark_strbuf_putc(html, 'x');
	return 1;
}

#pragma GCC diagnostic ignored "-Wunused-parameter"

char *cmark_render_markdown(cmark_node *root, int options)
{
	char *result;
	cmark_strbuf html = GH_BUF_INIT;
	cmark_event_type ev_type;
	cmark_node *cur;
	struct render_state state = { &html, NULL };
	cmark_iter *iter = cmark_iter_new(root);

	while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
		cur = cmark_iter_get_node(iter);
		S_render_node(cur, ev_type, &state);
	}
	result = (char *)cmark_strbuf_detach(&html);

	cmark_iter_free(iter);
	cmark_strbuf_free(&html);
	return result;
}

