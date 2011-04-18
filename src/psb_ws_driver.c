/*
 * INTEL CONFIDENTIAL
 * Copyright 2007 Intel Corporation. All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material may contain trade secrets and
 * proprietary and confidential information of Intel Corporation and its
 * suppliers and licensors, and is protected by worldwide copyright and trade
 * secret laws and treaty provisions. No part of the Material may be used,
 * copied, reproduced, modified, published, uploaded, posted, transmitted,
 * distributed, or disclosed in any way without Intel's prior express written
 * permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

/*
 * Authors:
 *    Binglin Chen <binglin.chen@intel.com>
 *
 */

#include "psb_def.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <linux/types.h>
#include <stdlib.h>
#include <string.h>
#include "psb_ws_driver.h"

static struct _ValidateNode *
psb_alloc(struct _WsbmVNodeFuncs *func, int type_id) {
    if (type_id == 0) {
        struct _PsbDrmValidateNode *vNode = malloc(sizeof(*vNode));

        if (vNode == NULL) return NULL;

        vNode->base.func = func;
        vNode->base.type_id = 0;
        return &vNode->base;
    } else {
        struct _ValidateNode *node = malloc(sizeof(*node));

        if (node == NULL) return NULL;

        node->func = func;
        node->type_id = 1;
        return node;
    }
}

/*
 * Free an allocated validate list node.
 */

static void
psb_free(struct _ValidateNode *node)
{
    if (node->type_id == 0)
        free(containerOf(node, struct _PsbDrmValidateNode, base));

    else
        free(node);
}

/*
 * Clear the private part of the validate list node. This happens when
 * the list node is newly allocated or is being reused. Since we only have
 * a private part when node->type_id == 0 we only care to clear in that
 * case. We want to clear the drm ioctl argument.
 */

static void
psb_clear(struct _ValidateNode *node)
{
    if (node->type_id == 0) {
        struct _PsbDrmValidateNode *vNode =
            containerOf(node, struct _PsbDrmValidateNode, base);

        memset(&vNode->val_arg.d.req, 0, sizeof(vNode->val_arg.d.req));
    }
}

static struct _WsbmVNodeFuncs psbVNode = {
    .alloc = psb_alloc,
    .free = psb_free,
    .clear = psb_clear,
};

struct _WsbmVNodeFuncs *
psbVNodeFuncs(void) {
    return &psbVNode;
}
