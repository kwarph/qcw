// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause


#include "DemoServices.h"
#include "QcwServiceRegistry.h"

namespace demo
{

IEditorService* editorService()
{
    return serviceRegistry->service<IEditorService>();
}

} // namespace demo
