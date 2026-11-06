// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "DemoNotes.h"
#include "QcwServiceRegistry.h"

namespace demo
{

INotesService* notesService()
{
    return serviceRegistry->service<INotesService>();
}

} // namespace demo
