/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include "kerncompat.h"
#include <stdlib.h>
#include <getopt.h>
#include "kernel-lib/list.h"
#include "common/messages.h"
#include "common/open-utils.h"
#include "common/parse-utils.h"
#include "common/help.h"
#include "cmds/commands.h"

static const char * const reflink_cmd_group_usage[] = {
	"btrfs reflnk <command> <args>",
	NULL
};

static const char * const cmd_reflink_clone_usage[] = {
	"btrfs reflink clone [options] source target",
	"Lightweight file copy",
	"Lightweight file copy, extents are cloned and COW if changed. Multiple",
	"ranges can be specified, source and target file can be the same,",
	"ranges can be combined from both and processed in the order.",
	"Options:",
	"  -s RANGESPEC        take range spec from the source file",
	"  -t RANGESPEC        take range from the target file",
	"",
	"RANGESPEC has three parts and is of format SRCOFF:LENGTH:DESTOFF,",
	"where SRCOFF is offset in the respective file, LENGTH is range length,",
	"DESTOFF is offset in the destination file (always target).",
	"All three values accept the size suffix (k/m/g/t/p/e, case insensitive).",
	NULL
};

struct reflink_range {
	struct list_head list;
	u64 from;
	u64 length;
	u64 to;
	bool same_file;
};

void parse_reflink_range(const char *str, u64 *from, u64 *length, u64 *to)
{
	char tmp[512];
	int i;

	/* Parse from */
	i = 0;
	while (*str && i < sizeof(tmp) && *str != ':')
		tmp[i++] = *str++;
	if (i >= sizeof(tmp)) {
		error("range spec too long");
		exit(1);
	}
	if (*str != ':') {
		error("wrong range spec near %s", str);
		exit(1);
	}
	*from = parse_size_from_string(tmp);
	str++;

	/* Parse length */
	i = 0;
	while (*str && i < sizeof(tmp) && *str != ':')
		tmp[i++] = *str++;
	if (i >= sizeof(tmp)) {
		error("range spec too long");
		exit(1);
	}
	if (*str != ':') {
		error("wrong range spec near %s", str);
		exit(1);
	}
	*length = parse_size_from_string(tmp);
	str++;

	/* Parse to, until end of string */
	*to = parse_size_from_string(str);
}

static int cmd_reflink_clone(const struct cmd_struct *cmd, int argc, char **argv)
{
	int ret = 0;
	LIST_HEAD(ranges);
	struct reflink_range *range = NULL, *tmp;

	optind = 0;
	while (1) {
		int c = getopt(argc, argv, "r:s:");
		bool same_file = false;

		if (c < 0)
			break;

		switch (c) {
		case 's':
			same_file = true;
			/* fallthrough */
		case 'r':
			range = malloc(sizeof(struct reflink_range));
			if (!range) {
				error("not enough memory");
				return 1;
			}
			INIT_LIST_HEAD(&range->list);
			range->same_file = same_file;
			parse_reflink_range(optarg, &range->from, &range->length, &range->to);
			list_add_tail(&range->list, &ranges);
			printf("NEW: %llu:%llu:%llu\n", range->from, range->length, range->to);
			break;
		default:
			usage_unknown_option(cmd, argv);
		}
	}

	if (check_argc_exact(argc - optind, 2))
		return 1;

	pr_verbose(2, "Source: %s\n", argv[optind]);
	pr_verbose(2, "Target: %s\n", argv[optind + 1]);

	list_for_each_entry(range, &ranges, list) {
		pr_verbose(2, "Range: %llu:%llu:%llu\n", range->from, range->length, range->to);
	}

	list_for_each_entry_safe(range, tmp, &ranges, list) {
		free(range);
	}

	return !!ret;
}
static DEFINE_SIMPLE_COMMAND(reflink_clone, "clone");

static const char reflink_cmd_group_info[] =
"reflink, shallow file copies: clone";

static const struct cmd_group reflink_cmd_group = {
	reflink_cmd_group_usage, reflink_cmd_group_info, {
		&cmd_struct_reflink_clone,
		NULL
	}
};

DEFINE_GROUP_COMMAND_TOKEN(reflink);
