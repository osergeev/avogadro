/**********************************************************************
  GLWidget - general OpenGL display

  Copyright (C) 2006 by Geoffrey R. Hutchison
  Some portions Copyright (C) 2006 by Donald E. Curtis

  This file is part of the Avogadro molecular editor project.
  For more information, see <http://avogadro.sourceforge.net/>

  Some code is based on Open Babel
  For more information, see <http://openbabel.sourceforge.net/>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 ***********************************************************************/

#include "GLWidget.h"

#include "MainWindow.h"

#include <stdio.h>

#define GL_SEL_BUF_SIZE 512

using namespace Avogadro;

GLWidget::GLWidget(QWidget *parent ) : QGLWidget(parent), defaultEngine(NULL), _clearColor(Qt::black)
{
  printf("Constructor\n");
  init();
}

GLWidget::GLWidget(const QGLFormat &format, QWidget *parent) : QGLWidget(format, parent), defaultEngine(NULL), _clearColor(Qt::black)
{
  printf("Constructor\n");
  init();
}

void GLWidget::init()
{
  _selectionDL = 0;
  loadEngines();

  molecule = NULL;
  view = new View(this); // nothing to include here
}

void GLWidget::initializeGL()
{
  printf("Initializing\n");

  qglClearColor ( _clearColor );

  glShadeModel( GL_SMOOTH );
  glEnable( GL_DEPTH_TEST );
  glDepthFunc( GL_LESS );
  glEnable( GL_CULL_FACE );
	glEnable( GL_COLOR_SUM_EXT );

  GLfloat mat_ambient[] = { 0.5, 0.5, 0.5, 1.0 };
  GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
  GLfloat mat_shininess[] = { 30.0 };

  glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
  glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
  glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
  glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

  // Used to display semi-transparent relection rectangle
  //  glBlendFunc(GL_ONE, GL_ONE);
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

  glEnable( GL_NORMALIZE );
  glEnable( GL_LIGHTING );

  GLfloat ambientLight[] = { 0.4, 0.4, 0.4, 1.0 };
  GLfloat diffuseLight[] = { 0.8, 0.8, 0.8, 1.0 };
  GLfloat specularLight[] = { 1.0, 1.0, 1.0, 1.0 };
  GLfloat position[] = { 0.8, 0.7, 1.0, 0.0 };

	glLightModeli( GL_LIGHT_MODEL_COLOR_CONTROL_EXT,
                 GL_SEPARATE_SPECULAR_COLOR_EXT );
  glLightfv( GL_LIGHT0, GL_AMBIENT, ambientLight );
  glLightfv( GL_LIGHT0, GL_DIFFUSE, diffuseLight );
  glLightfv( GL_LIGHT0, GL_SPECULAR, specularLight );
  glLightfv( GL_LIGHT0, GL_POSITION, position );
  glEnable( GL_LIGHT0 );

  glMatrixMode(GL_MODELVIEW);

  // Initialize a clean rotation matrix
  glPushMatrix();
  glLoadIdentity();
  glGetDoublev( GL_MODELVIEW_MATRIX, _RotationMatrix);
  glPopMatrix();

  _TranslationVector[0] = 0.0;
  _TranslationVector[1] = 0.0;
  _TranslationVector[2] = 0.0;

  _Scale = 1.0;

  glEnableClientState( GL_VERTEX_ARRAY );
  glEnableClientState( GL_NORMAL_ARRAY );
}

void GLWidget::resizeGL(int width, int height)
{
  printf("Resizing.\n");
  glViewport(0,0,width,height);

}

void GLWidget::setCamera()
{
  // Reset the projection and set our perspective.
  gluPerspective(35,float(width())/height(),0.1,1000);

  // pull the camera back 20
  glTranslated ( 0.0, 0.0, -20.0 );
}

void GLWidget::render(GLenum mode)
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // TODO: Be careful here.  For the time being we are actually changing the
  // orientation of our render in 3d space and not changing the camera.  And also
  // we're not changing the coordinates of the atoms.
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Translate our molecule as per user instructions
  glTranslated(_TranslationVector[0], _TranslationVector[1], _TranslationVector[2]);
  glScaled(_Scale, _Scale, _Scale);
  glMultMatrixd(_RotationMatrix);

  view->render();

  glCallList(_selectionDL);

  glFlush();
}

void GLWidget::paintGL()
{ 
//X   printf("Painting.\n");

  // Reset the projection
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  setCamera();

  // Reset the model view
  glMatrixMode(GL_MODELVIEW);
  render(GL_RENDER);
}

void GLWidget::mousePressEvent( QMouseEvent * event )
{
  // See if this click has hit any in our molecule
  // Offset by 2 because we want the cursor to be in the middle of the region.
  selectRegion(event->pos().x()-2, event->pos().y()-2, 5, 5);

  _movedSinceButtonPressed = false;
  _lastDraggingPosition = event->pos ();
  _initialDraggingPosition = event->pos ();
}

void GLWidget::mouseReleaseEvent( QMouseEvent * event )
{
  // TODO: Fix this!
  if(_selectionDL)
  {
    glNewList(_selectionDL, GL_COMPILE);
    glEndList();
  }

  if(!_movedSinceButtonPressed && _hits.size())
  {
    for(int i=0; i < _hits.size(); i++) {
      if(_hits[i].type == atomType)
      {
        ((Atom *)molecule->GetAtom(_hits[i].name))->toggleSelected();
        break;
      }
    }
  }
  else if(_movedSinceButtonPressed && !_hits.size())
  {
    int sx = qMin(_initialDraggingPosition.x(), _lastDraggingPosition.x());
    int ex = qMax(_initialDraggingPosition.x(), _lastDraggingPosition.x());
    int sy = qMin(_initialDraggingPosition.y(), _lastDraggingPosition.y());
    int ey = qMax(_initialDraggingPosition.y(), _lastDraggingPosition.y());
    qDebug("(%d, %d)", _initialDraggingPosition.x(),_initialDraggingPosition.y());
    qDebug("(%d, %d)", _lastDraggingPosition.x(),_lastDraggingPosition.y());
    qDebug("(%d, %d)", sx, sy);
    qDebug("(%d, %d)", ex, ey);
    int w = ex-sx;
    int h = ey-sy;
    // (sx, sy) = Upper left most position.
    // (ex, ey) = Bottom right most position.
    selectRegion(sx, sy, w, h);
    for(int i=0; i < _hits.size(); i++) {
      if(_hits[i].type == atomType)
      {
        ((Atom *)molecule->GetAtom(_hits[i].name))->toggleSelected();
      }
    }
  }

  updateGL();
}

void GLWidget::mouseMoveEvent( QMouseEvent * event )
{
  QPoint deltaDragging = event->pos() - _lastDraggingPosition;
  _lastDraggingPosition = event->pos();
  if( ( event->pos()
        - _initialDraggingPosition ).manhattanLength() > 2 )
    _movedSinceButtonPressed = true;

  if( _hits.size() )
  {
    if( event->buttons() & Qt::LeftButton )
    {
      glPushMatrix();
      glLoadIdentity();
      glRotated( deltaDragging.x(), 0.0, 1.0, 0.0 );
      glRotated( deltaDragging.y(), 1.0, 0.0, 0.0 );
      glMultMatrixd( _RotationMatrix );
      glGetDoublev( GL_MODELVIEW_MATRIX, _RotationMatrix );
      glPopMatrix();
    }
    else if ( event->buttons() & Qt::RightButton )
    {
      deltaDragging = _initialDraggingPosition - event->pos();

      _TranslationVector[0] = -deltaDragging.x() / 5.0;
      _TranslationVector[1] = deltaDragging.y() / 5.0;
    }
    else if ( event->buttons() & Qt::MidButton )
    {
      deltaDragging = _initialDraggingPosition - event->pos();
      int xySum = deltaDragging.x() + deltaDragging.y();

      if (xySum < 0)
        _Scale = deltaDragging.manhattanLength() / 5.0;
      else if (xySum > 0)
        _Scale = 1.0 / deltaDragging.manhattanLength();
    }
  }
  else
  {
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT,viewport);
    selectionBox(_initialDraggingPosition.x(), _initialDraggingPosition.y(),
        _lastDraggingPosition.x(), _lastDraggingPosition.y());
  }

  updateGL();
}

void GLWidget::selectionBox(int sx, int sy, int ex, int ey)
{
  // XXX: There has to be a better way to do this
  // besides display lists.  Probably vertex arrays.
  // Enough for tonite.  Clean later.
  if(!_selectionDL)
  {
    _selectionDL = glGenLists(1);
  }

  glPushMatrix();
  glLoadIdentity();
  GLdouble projection[16];
  glGetDoublev(GL_PROJECTION_MATRIX,projection);
  GLdouble modelview[16];
  glGetDoublev(GL_MODELVIEW_MATRIX,modelview);
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT,viewport);

  GLdouble startPos[3];
  GLdouble endPos[3];

  gluUnProject(float(sx), viewport[3] - float(sy), 0.1, modelview, projection, viewport, &startPos[0], &startPos[1], &startPos[2]);
  gluUnProject(float(ex), viewport[3] - float(ey), 0.1, modelview, projection, viewport, &endPos[0], &endPos[1], &endPos[2]);

  qDebug("(%f, %f, %f)", endPos[0],endPos[1],endPos[2]);

  glNewList(_selectionDL, GL_COMPILE);
  glMatrixMode(GL_MODELVIEW);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glPushMatrix();
  glLoadIdentity();
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  Color(1.0, 1.0, 1.0, 0.2).applyAsMaterials();
  glBegin(GL_POLYGON);
  glVertex3f(startPos[0],startPos[1],startPos[2]);
  glVertex3f(startPos[0],endPos[1],startPos[2]);
  glVertex3f(endPos[0],endPos[1],startPos[2]);
  glVertex3f(endPos[0],startPos[1],startPos[2]);
  glEnd();
  startPos[2] += 0.0001;
  Color(1.0, 1.0, 1.0, 1.0).applyAsMaterials();
  glBegin(GL_LINE_LOOP);
  glVertex3f(startPos[0],startPos[1],startPos[2]);
  glVertex3f(startPos[0],endPos[1],startPos[2]);
  glVertex3f(endPos[0],endPos[1],startPos[2]);
  glVertex3f(endPos[0],startPos[1],startPos[2]);
  glEnd();
  glPopMatrix();
  glPopAttrib();
  glEndList();

  glPopMatrix();

}

void GLWidget::selectRegion(int x, int y, int w, int h)
{
  GLuint selectBuf[GL_SEL_BUF_SIZE];
  GLint viewport[4];
  int hits;

  int cx = w/2 + x;
  int cy = h/2 + y;

  _hits.clear();

  glSelectBuffer(GL_SEL_BUF_SIZE,selectBuf);
  glRenderMode(GL_SELECT);

  // Setup our limited viewport for picking.
  glGetIntegerv(GL_VIEWPORT,viewport);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluPickMatrix(cx,viewport[3]-cy, w, h,viewport);

  setCamera();

  // Get ready for rendering
  glMatrixMode(GL_MODELVIEW);
  glInitNames();
  render(GL_SELECT);

  // restoring the original projection matrix
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glFlush();

  // returning to normal rendering mode
  hits = glRenderMode(GL_RENDER);

  // if there are hits process them
  if (hits != 0)
    updateHitList(hits,selectBuf);
}

void GLWidget::updateHitList(GLint hits, GLuint buffer[])
{
  unsigned int i, j;
  GLuint names, type, *ptr;
  GLuint minZ, maxZ, name;

//X   printf ("hits = %d\n", hits);
  ptr = (GLuint *) buffer;
  for (i = 0; i < hits; i++) {
    names = *ptr++;
    minZ = *ptr++;
    maxZ = *ptr++;
//X     printf (" number of names for this hit = %d\n", names); names;
//X     printf("  z1 is %g;", (float) *ptr/0x7fffffff); minZ;
//X     printf(" z2 is %g\n", (float) *ptr/0x7fffffff); maxZ;
//X     printf ("   names are "); 
    name = 0;
    for (j = 0; j < names/2; j++) { /*  for each name */
      type = *ptr++;
      name = *ptr++;
      printf ("%d(%d)", name,type);
//X       if (j == 0)  /*  set row and column  */
//X         ii = *ptr;
//X       else if (j == 1)
//X         jj = *ptr;
//X       ptr++;
    }
    if (name)
    {
      _hits.append(GLHit(name, type, minZ, maxZ));
    }
//X     printf ("\n");
  }
  qSort(_hits);
}

void GLWidget::setView(View *v)
{
  view = v;
}

void GLWidget::setMolecule(Molecule *m)
{
  if (molecule)
    delete molecule;

  molecule = m;

  if (view)
    delete view;

  view = new MoleculeView(molecule, this);
}

void GLWidget::setDefaultEngine(int i) 
{
  setDefaultEngine(glEngines.at(i));
}

void GLWidget::setDefaultEngine(Engine *e) 
{
  if(e)
  {
    defaultEngine = e;
    updateGL();
  }
}

void GLWidget::loadEngines()
{
  QDir pluginsDir = QDir(qApp->applicationDirPath());

  if (!pluginsDir.cd("engines") && getenv("AVOGADRO_PLUGINS") != NULL)
  {
    pluginsDir.cd(getenv("AVOGADRO_PLUGINS"));
  }

  qDebug() << "PluginsDir:" << pluginsDir.absolutePath() << endl;
  // load static plugins first
//   foreach (QObject *plugin, QPluginLoader::staticInstances())
//   {
//     Engine *r = qobject_cast<Engine *>(plugin);
//     if (r)
//     {
//       qDebug() << "Loaded Engine: " << r->name() << endl;
//       if( defaultEngine == NULL )
//         defaultEngine = r;
//     }
//   }

  foreach (QString fileName, pluginsDir.entryList(QDir::Files)) {
    QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
    EngineFactory *factory = qobject_cast<EngineFactory *>(loader.instance());
    if (factory) {
      Engine *engine = factory->createInstance();
      qDebug() << "Found Plugin: " << engine->name() << " - " << engine->description(); 
      if (!defaultEngine)
      {
        qDebug() << "Setting Default Engine: " << engine->name() << " - " << engine->description(); 
        defaultEngine = engine;
      }
      glEngines.append(engine);
    }
  }
}

