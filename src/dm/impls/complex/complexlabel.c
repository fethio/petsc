#include <petsc-private/compleximpl.h>   /*I      "petscdmcomplex.h"   I*/

#undef __FUNCT__
#define __FUNCT__ "DMLabelCreate"
PetscErrorCode DMLabelCreate(const char name[], DMLabel *label)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscNew(struct _n_DMLabel, label);CHKERRQ(ierr);
  ierr = PetscStrallocpy(name, &(*label)->name);CHKERRQ(ierr);
  (*label)->refct          = 1;
  (*label)->stratumValues  = PETSC_NULL;
  (*label)->stratumOffsets = PETSC_NULL;
  (*label)->stratumSizes   = PETSC_NULL;
  (*label)->points         = PETSC_NULL;
  (*label)->next           = PETSC_NULL;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMLabelView_Ascii"
static PetscErrorCode DMLabelView_Ascii(DMLabel label, PetscViewer viewer)
{
  PetscInt        v;
  PetscMPIInt     rank;
  PetscErrorCode  ierr;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(((PetscObject) viewer)->comm, &rank);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer, "Label '%s':\n", label->name);CHKERRQ(ierr);
  for (v = 0; v < label->numStrata; ++v) {
    const PetscInt value = label->stratumValues[v];
    PetscInt       p;

    for(p = label->stratumOffsets[v]; p < label->stratumOffsets[v]+label->stratumSizes[v]; ++p) {
      ierr = PetscViewerASCIISynchronizedPrintf(viewer, "[%D]: %D (%D)\n", rank, label->points[p], value);CHKERRQ(ierr);
    }
  }
  ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMLabelView"
PetscErrorCode DMLabelView(DMLabel label, PetscViewer viewer)
{
  PetscBool      iascii;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(viewer, PETSC_VIEWER_CLASSID, 2);
  ierr = PetscObjectTypeCompare((PetscObject) viewer, PETSCVIEWERASCII, &iascii);CHKERRQ(ierr);
  if (iascii) {
    ierr = DMLabelView_Ascii(label, viewer);CHKERRQ(ierr);
  } else SETERRQ1(((PetscObject) viewer)->comm, PETSC_ERR_SUP, "Viewer type %s not supported by this mesh object", ((PetscObject) viewer)->type_name);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMLabelDestroy"
PetscErrorCode DMLabelDestroy(DMLabel *label)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (--(*label)->refct > 0) {PetscFunctionReturn(0);}
  ierr = PetscFree((*label)->name);CHKERRQ(ierr);
  ierr = PetscFree3((*label)->stratumValues,(*label)->stratumOffsets,(*label)->stratumSizes);CHKERRQ(ierr);
  ierr = PetscFree((*label)->points);CHKERRQ(ierr);
  ierr = PetscFree(*label);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMLabelGetValue"
PetscErrorCode DMLabelGetValue(DMLabel label, PetscInt point, PetscInt *value)
{
  PetscInt       v;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(value, 3);
  *value = -1;
  for(v = 0; v < label->numStrata; ++v) {
    PetscInt i;

    ierr = PetscFindInt(point, label->stratumSizes[v], &label->points[label->stratumOffsets[v]], &i);
    if (i >= 0) {
      *value = label->stratumValues[v];
      break;
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMLabelSetValue"
PetscErrorCode DMLabelSetValue(DMLabel label, PetscInt point, PetscInt value)
{
  PetscInt       v, loc;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* Find, or add, label value */
  for (v = 0; v < label->numStrata; ++v) {
    if (label->stratumValues[v] == value) break;
  }
  if (v >= label->numStrata) {
    PetscInt *tmpV, *tmpO, *tmpS;

    ierr = PetscMalloc3(label->numStrata+1,PetscInt,&tmpV,label->numStrata+2,PetscInt,&tmpO,label->numStrata+1,PetscInt,&tmpS);CHKERRQ(ierr);
    for (v = 0; v < label->numStrata; ++v) {
      tmpV[v] = label->stratumValues[v];
      tmpO[v] = label->stratumOffsets[v];
      tmpS[v] = label->stratumSizes[v];
    }
    tmpV[v] = value;
    tmpO[v] = v == 0 ? 0 : label->stratumOffsets[v];
    tmpS[v] = 0;
    tmpO[v+1] = tmpO[v];
    ++label->numStrata;
    ierr = PetscFree3(label->stratumValues,label->stratumOffsets,label->stratumSizes);CHKERRQ(ierr);
    label->stratumValues  = tmpV;
    label->stratumOffsets = tmpO;
    label->stratumSizes   = tmpS;
  }
  /* Check whether point exists */
  ierr = PetscFindInt(point,label->stratumSizes[v],label->points+label->stratumOffsets[v],&loc);CHKERRQ(ierr);
  if (loc < 0) {
    PetscInt off = label->stratumOffsets[v] - (loc+1); /* decode insert location */
    /* Check for reallocation */
    if (label->stratumSizes[v] >= label->stratumOffsets[v+1]-label->stratumOffsets[v]) {
      PetscInt  oldSize   = label->stratumOffsets[v+1]-label->stratumOffsets[v];
      PetscInt  newSize   = PetscMax(10, 2*oldSize); /* Double the size, since 2 is the optimal base for this online algorithm */
      PetscInt  shift     = newSize - oldSize;
      PetscInt  allocSize = label->stratumOffsets[label->numStrata] + shift;
      PetscInt *newPoints;
      PetscInt  w, q;

      ierr = PetscMalloc(allocSize * sizeof(PetscInt), &newPoints);CHKERRQ(ierr);
      for (q = 0; q < label->stratumOffsets[v]+label->stratumSizes[v]; ++q) {
        newPoints[q] = label->points[q];
      }
      for (w = v+1; w < label->numStrata; ++w) {
        for (q = label->stratumOffsets[w]; q < label->stratumOffsets[w]+label->stratumSizes[w]; ++q) {
          newPoints[q+shift] = label->points[q];
        }
        label->stratumOffsets[w] += shift;
      }
      label->stratumOffsets[label->numStrata] += shift;
      ierr = PetscFree(label->points);CHKERRQ(ierr);
      label->points = newPoints;
    }
    ierr = PetscMemmove(&label->points[off+1], &label->points[off], (label->stratumSizes[v]+(loc+1)) * sizeof(PetscInt));CHKERRQ(ierr);
    label->points[off] = point;
    ++label->stratumSizes[v];
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMLabelClearValue"
PetscErrorCode DMLabelClearValue(DMLabel label, PetscInt point, PetscInt value)
{
  PetscInt v, p;

  PetscFunctionBegin;
  /* Find label value */
  for(v = 0; v < label->numStrata; ++v) {
    if (label->stratumValues[v] == value) break;
  }
  if (v >= label->numStrata) PetscFunctionReturn(0);
  /* Check whether point exists */
  for (p = label->stratumOffsets[v]; p < label->stratumOffsets[v]+label->stratumSizes[v]; ++p) {
    if (label->points[p] == point) {
      /* Found point */
      PetscInt  q;

      for (q = p+1; q < label->stratumOffsets[v]+label->stratumSizes[v]; ++q) {
        label->points[q-1] = label->points[q];
      }
      --label->stratumSizes[v];
      break;
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMLabelGetNumValues"
PetscErrorCode DMLabelGetNumValues(DMLabel label, PetscInt *numValues)
{
  PetscFunctionBegin;
  PetscValidPointer(numValues, 2);
  *numValues = label->numStrata;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMLabelGetValueIS"
PetscErrorCode DMLabelGetValueIS(DMLabel label, IS *values)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(values, 2);
  ierr = ISCreateGeneral(PETSC_COMM_SELF, label->numStrata, label->stratumValues, PETSC_COPY_VALUES, values);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMLabelGetStratumSize"
PetscErrorCode DMLabelGetStratumSize(DMLabel label, PetscInt value, PetscInt *size)
{
  PetscInt v;

  PetscFunctionBegin;
  PetscValidPointer(size, 3);
  *size = 0;
  for(v = 0; v < label->numStrata; ++v) {
    if (label->stratumValues[v] == value) {
      *size = label->stratumSizes[v];
      break;
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMLabelGetStratumIS"
PetscErrorCode DMLabelGetStratumIS(DMLabel label, PetscInt value, IS *points)
{
  PetscInt       v;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(points, 3);
  *points = PETSC_NULL;
  for(v = 0; v < label->numStrata; ++v) {
    if (label->stratumValues[v] == value) {
      ierr = ISCreateGeneral(PETSC_COMM_SELF, label->stratumSizes[v], &label->points[label->stratumOffsets[v]], PETSC_COPY_VALUES, points);CHKERRQ(ierr);
      break;
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMLabelClearStratum"
PetscErrorCode DMLabelClearStratum(DMLabel label, PetscInt value)
{
  PetscInt v;

  PetscFunctionBegin;
  for(v = 0; v < label->numStrata; ++v) {
    if (label->stratumValues[v] == value) break;
  }
  if (v >= label->numStrata) PetscFunctionReturn(0);
  label->stratumSizes[v] = 0;
  PetscFunctionReturn(0);
}



#undef __FUNCT__
#define __FUNCT__ "DMComplexCreateLabel"
/*@C
  DMComplexCreateLabel - Create a label of the given name if it does not already exist

  Not Collective

  Input Parameters:
+ dm   - The DMComplex object
- name - The label name

  Level: intermediate

.keywords: mesh
.seealso: DMLabelCreate(), DMComplexHasLabel(), DMComplexGetLabelValue(), DMComplexSetLabelValue(), DMComplexGetStratumIS()
@*/
PetscErrorCode DMComplexCreateLabel(DM dm, const char name[])
{
  DM_Complex    *mesh = (DM_Complex *) dm->data;
  DMLabel        next = mesh->labels;
  PetscBool      flg  = PETSC_FALSE;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  while(next) {
    ierr = PetscStrcmp(name, next->name, &flg);CHKERRQ(ierr);
    if (flg) break;
    next = next->next;
  }
  if (!flg) {
    DMLabel tmpLabel = mesh->labels;
    ierr = DMLabelCreate(name, &mesh->labels);CHKERRQ(ierr);
    mesh->labels->next = tmpLabel;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexGetLabelValue"
/*@C
  DMComplexGetLabelValue - Get the value in a Sieve Label for the given point, with 0 as the default

  Not Collective

  Input Parameters:
+ dm   - The DMComplex object
. name - The label name
- point - The mesh point

  Output Parameter:
. value - The label value for this point, or -1 if the point is not in the label

  Level: beginner

.keywords: mesh
.seealso: DMLabelGetValue(), DMComplexSetLabelValue(), DMComplexGetStratumIS()
@*/
PetscErrorCode DMComplexGetLabelValue(DM dm, const char name[], PetscInt point, PetscInt *value)
{
  DMLabel        label;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  ierr = DMComplexGetLabel(dm, name, &label);CHKERRQ(ierr);
  if (!label) SETERRQ1(((PetscObject) dm)->comm, PETSC_ERR_ARG_WRONG, "No label named %s was found", name);CHKERRQ(ierr);
  ierr = DMLabelGetValue(label, point, value);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexSetLabelValue"
/*@C
  DMComplexSetLabelValue - Add a point to a Sieve Label with given value

  Not Collective

  Input Parameters:
+ dm   - The DMComplex object
. name - The label name
. point - The mesh point
- value - The label value for this point

  Output Parameter:

  Level: beginner

.keywords: mesh
.seealso: DMLabelSetValue(), DMComplexGetStratumIS(), DMComplexClearLabelValue()
@*/
PetscErrorCode DMComplexSetLabelValue(DM dm, const char name[], PetscInt point, PetscInt value)
{
  DMLabel        label;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  ierr = DMComplexGetLabel(dm, name, &label);CHKERRQ(ierr);
  if (!label) {
    ierr = DMComplexCreateLabel(dm, name);CHKERRQ(ierr);
    ierr = DMComplexGetLabel(dm, name, &label);CHKERRQ(ierr);
  }
  ierr = DMLabelSetValue(label, point, value);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexClearLabelValue"
/*@C
  DMComplexClearLabelValue - Remove a point from a Sieve Label with given value

  Not Collective

  Input Parameters:
+ dm   - The DMComplex object
. name - The label name
. point - The mesh point
- value - The label value for this point

  Output Parameter:

  Level: beginner

.keywords: mesh
.seealso: DMLabelClearValue(), DMComplexSetLabelValue(), DMComplexGetStratumIS()
@*/
PetscErrorCode DMComplexClearLabelValue(DM dm, const char name[], PetscInt point, PetscInt value)
{
  DMLabel        label;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  ierr = DMComplexGetLabel(dm, name, &label);CHKERRQ(ierr);
  if (!label) PetscFunctionReturn(0);
  ierr = DMLabelClearValue(label, point, value);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexGetLabelSize"
/*@C
  DMComplexGetLabelSize - Get the number of different integer ids in a Label

  Not Collective

  Input Parameters:
+ dm   - The DMComplex object
- name - The label name

  Output Parameter:
. size - The number of different integer ids, or 0 if the label does not exist

  Level: beginner

.keywords: mesh
.seealso: DMLabeGetNumValues(), DMComplexSetLabelValue()
@*/
PetscErrorCode DMComplexGetLabelSize(DM dm, const char name[], PetscInt *size)
{
  DMLabel        label;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  PetscValidPointer(size, 3);
  ierr = DMComplexGetLabel(dm, name, &label);CHKERRQ(ierr);
  *size = 0;
  if (!label) PetscFunctionReturn(0);
  ierr = DMLabelGetNumValues(label, size);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexGetLabelIdIS"
/*@C
  DMComplexGetLabelIdIS - Get the integer ids in a label

  Not Collective

  Input Parameters:
+ mesh - The DMComplex object
- name - The label name

  Output Parameter:
. ids - The integer ids, or PETSC_NULL if the label does not exist

  Level: beginner

.keywords: mesh
.seealso: DMLabelGetValueIS(), DMComplexGetLabelSize()
@*/
PetscErrorCode DMComplexGetLabelIdIS(DM dm, const char name[], IS *ids)
{
  DMLabel        label;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  PetscValidPointer(ids, 3);
  ierr = DMComplexGetLabel(dm, name, &label);CHKERRQ(ierr);
  *ids = PETSC_NULL;
  if (!label) PetscFunctionReturn(0);
  ierr = DMLabelGetValueIS(label, ids);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexGetStratumSize"
/*@C
  DMComplexGetStratumSize - Get the number of points in a label stratum

  Not Collective

  Input Parameters:
+ dm - The DMComplex object
. name - The label name
- value - The stratum value

  Output Parameter:
. size - The stratum size

  Level: beginner

.keywords: mesh
.seealso: DMLabelGetStratumSize(), DMComplexGetLabelSize(), DMComplexGetLabelIds()
@*/
PetscErrorCode DMComplexGetStratumSize(DM dm, const char name[], PetscInt value, PetscInt *size)
{
  DMLabel        label;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  PetscValidPointer(size, 4);
  ierr = DMComplexGetLabel(dm, name, &label);CHKERRQ(ierr);
  *size = 0;
  if (!label) PetscFunctionReturn(0);
  ierr = DMLabelGetStratumSize(label, value, size);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexGetStratumIS"
/*@C
  DMComplexGetStratumIS - Get the points in a label stratum

  Not Collective

  Input Parameters:
+ dm - The DMComplex object
. name - The label name
- value - The stratum value

  Output Parameter:
. points - The stratum points, or PETSC_NULL if the label does not exist or does not have that value

  Level: beginner

.keywords: mesh
.seealso: DMLabelGetStratumIS(), DMComplexGetStratumSize()
@*/
PetscErrorCode DMComplexGetStratumIS(DM dm, const char name[], PetscInt value, IS *points) {
  DMLabel        label;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  PetscValidPointer(points, 4);
  ierr = DMComplexGetLabel(dm, name, &label);CHKERRQ(ierr);
  *points = PETSC_NULL;
  if (!label) PetscFunctionReturn(0);
  ierr = DMLabelGetStratumIS(label, value, points);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexClearLabelStratum"
/*@C
  DMComplexClearLabelStratum - Remove all points from a stratum from a Sieve Label

  Not Collective

  Input Parameters:
+ dm   - The DMComplex object
. name - The label name
- value - The label value for this point

  Output Parameter:

  Level: beginner

.keywords: mesh
.seealso: DMLabelClearStratum(), DMComplexSetLabelValue(), DMComplexGetStratumIS(), DMComplexClearLabelValue()
@*/
PetscErrorCode DMComplexClearLabelStratum(DM dm, const char name[], PetscInt value)
{
  DMLabel        label;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  ierr = DMComplexGetLabel(dm, name, &label);CHKERRQ(ierr);
  if (!label) PetscFunctionReturn(0);
  ierr = DMLabelClearStratum(label, value);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}



#undef __FUNCT__
#define __FUNCT__ "DMComplexGetNumLabels"
/*@
  DMComplexGetNumLabels - Return the number of labels defined by the mesh

  Not Collective

  Input Parameter:
. dm   - The DMComplex object

  Output Parameter:
. numLabels - the number of Labels

  Level: intermediate

.keywords: mesh
.seealso: DMComplexGetLabelValue(), DMComplexSetLabelValue(), DMComplexGetStratumIS()
@*/
PetscErrorCode DMComplexGetNumLabels(DM dm, PetscInt *numLabels)
{
  DM_Complex *mesh = (DM_Complex *) dm->data;
  DMLabel     next = mesh->labels;
  PetscInt    n    = 0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidPointer(numLabels, 2);
  while(next) {
    ++n;
    next = next->next;
  }
  *numLabels = n;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexGetLabelName"
/*@C
  DMComplexGetLabelName - Return the name of nth label

  Not Collective

  Input Parameters:
+ dm - The DMComplex object
- n  - the label number

  Output Parameter:
. name - the label name

  Level: intermediate

.keywords: mesh
.seealso: DMComplexGetLabelValue(), DMComplexSetLabelValue(), DMComplexGetStratumIS()
@*/
PetscErrorCode DMComplexGetLabelName(DM dm, PetscInt n, const char **name)
{
  DM_Complex *mesh = (DM_Complex *) dm->data;
  DMLabel     next = mesh->labels;
  PetscInt    l    = 0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 3);
  while(next) {
    if (l == n) {
      *name = next->name;
      PetscFunctionReturn(0);
    }
    ++l;
    next = next->next;
  }
  SETERRQ1(((PetscObject) dm)->comm, PETSC_ERR_ARG_OUTOFRANGE, "Label %d does not exist in this DM", n);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexHasLabel"
/*@C
  DMComplexHasLabel - Determine whether the mesh has a label of a given name

  Not Collective

  Input Parameters:
+ dm   - The DMComplex object
- name - The label name

  Output Parameter:
. hasLabel - PETSC_TRUE if the label is present

  Level: intermediate

.keywords: mesh
.seealso: DMComplexCreateLabel(), DMComplexGetLabelValue(), DMComplexSetLabelValue(), DMComplexGetStratumIS()
@*/
PetscErrorCode DMComplexHasLabel(DM dm, const char name[], PetscBool *hasLabel)
{
  DM_Complex    *mesh = (DM_Complex *) dm->data;
  DMLabel        next = mesh->labels;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  PetscValidPointer(hasLabel, 3);
  *hasLabel = PETSC_FALSE;
  while(next) {
    ierr = PetscStrcmp(name, next->name, hasLabel);CHKERRQ(ierr);
    if (*hasLabel) break;
    next = next->next;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexGetLabel"
/*@C
  DMComplexGetLabel - Return the label of a given name, or PETSC_NULL

  Not Collective

  Input Parameters:
+ dm   - The DMComplex object
- name - The label name

  Output Parameter:
. label - The DMLabel, or PETSC_NULL if the label is absent

  Level: intermediate

.keywords: mesh
.seealso: DMComplexCreateLabel(), DMComplexHasLabel(), DMComplexGetLabelValue(), DMComplexSetLabelValue(), DMComplexGetStratumIS()
@*/
PetscErrorCode DMComplexGetLabel(DM dm, const char name[], DMLabel *label)
{
  DM_Complex    *mesh = (DM_Complex *) dm->data;
  DMLabel        next = mesh->labels;
  PetscBool      hasLabel;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  PetscValidCharPointer(name, 2);
  PetscValidPointer(label, 3);
  *label = PETSC_NULL;
  while(next) {
    ierr = PetscStrcmp(name, next->name, &hasLabel);CHKERRQ(ierr);
    if (hasLabel) {
      *label = next;
      break;
    }
    next = next->next;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexAddLabel"
/*@C
  DMComplexAddLabel - Add the label to this mesh

  Not Collective

  Input Parameters:
+ dm   - The DMComplex object
- label - The DMLabel

  Level: developer

.keywords: mesh
.seealso: DMComplexCreateLabel(), DMComplexHasLabel(), DMComplexGetLabelValue(), DMComplexSetLabelValue(), DMComplexGetStratumIS()
@*/
PetscErrorCode DMComplexAddLabel(DM dm, DMLabel label)
{
  DM_Complex    *mesh = (DM_Complex *) dm->data;
  PetscBool      hasLabel;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  ierr = DMComplexHasLabel(dm, label->name, &hasLabel);CHKERRQ(ierr);
  if (hasLabel) SETERRQ1(((PetscObject) dm)->comm, PETSC_ERR_ARG_OUTOFRANGE, "Label %s already exists in this DM", label->name);
  label->next  = mesh->labels;
  mesh->labels = label;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMComplexRemoveLabel"
/*@C
  DMComplexRemoveLabel - Remove the label from this mesh

  Not Collective

  Input Parameters:
+ dm   - The DMComplex object
- name - The label name

  Output Parameter:
. label - The DMLabel, or PETSC_NULL if the label is absent

  Level: developer

.keywords: mesh
.seealso: DMComplexCreateLabel(), DMComplexHasLabel(), DMComplexGetLabelValue(), DMComplexSetLabelValue(), DMComplexGetStratumIS()
@*/
PetscErrorCode DMComplexRemoveLabel(DM dm, const char name[], DMLabel *label)
{
  DM_Complex    *mesh = (DM_Complex *) dm->data;
  DMLabel        next = mesh->labels;
  DMLabel        last = PETSC_NULL;
  PetscBool      hasLabel;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm, DM_CLASSID, 1);
  ierr = DMComplexHasLabel(dm, name, &hasLabel);CHKERRQ(ierr);
  *label = PETSC_NULL;
  if (!hasLabel) PetscFunctionReturn(0);
  while(next) {
    ierr = PetscStrcmp(name, next->name, &hasLabel);CHKERRQ(ierr);
    if (hasLabel) {
      if (last) last->next = next->next;
      next->next = PETSC_NULL;
      *label = next;
      break;
    }
    last = next;
    next = next->next;
  }
  PetscFunctionReturn(0);
}
