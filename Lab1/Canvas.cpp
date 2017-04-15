#include "Canvas.hpp"
#include <fstream>
#include "Lab2Primitive.hpp"
#include "Lab3Primitive.hpp"
#include "Lab4Primitives.hpp"
#include "Lab6Primitive.hpp"
#include "SquareCircle.h"
#include <qevent.h>
#include "MovementHolder.hpp"

Canvas::Canvas() : foreground(1.f), background(1.f), element(nullptr), elementN(nullptr), isMouseLocked(false), isMovementHolderInserted(false), useNormals(false) {
	buffers = new GLuint[1];
	resetCamera();
}

Canvas::~Canvas() {
	if (buffers) delete[] buffers;
	if (element) delete element;
	if (elementN) delete elementN;
}

void Canvas::initializeGL() {
 	initializeOpenGLFunctions();
	linkPrograms();

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glEnable(GL_POINT_SMOOTH);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	glEnable(GL_POLYGON_SMOOTH);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

	glEnableVertexAttribArray(0);

	createSquareCircle();
}
void Canvas::resizeGL(int w, int h) {
	glViewport(0, 0, w, h);
	QMatrix4x4 projection;
	projection.perspective(60, float(w) / h, 0.01f, 100.f);
	glUseProgram(program);
	glUniformMatrix4fv(locs.projectionMatrixLoc, 1, GL_FALSE, projection.data());
	updateLookAt();
}
void Canvas::paintGL() {
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(program);
	if (useNormals)
		drawElement(elementN, coordinates);
	else
		drawElement(element, coordinates);
}

void Canvas::mousePressEvent(QMouseEvent * e) {
	isMouseLocked = !isMouseLocked;
	setMouseTracking(isMouseLocked);
	if (isMouseLocked) {
		QCursor::setPos(mapToGlobal(QPoint(width() / 2, height() / 2)));
		setCursor(Qt::BlankCursor);
	} else
		setCursor(Qt::ArrowCursor);
}

void Canvas::mouseMoveEvent(QMouseEvent * e) {
	if (isMouseLocked) {
		float x = e->x() - width() / 2;
		float y = e->y() - height() / 2;
		lookPoint += lookPoint * upVector * x / height();
		lookPoint -= Point(0, 0, 1) * upVector * lookPoint * y / height();
		QCursor::setPos(mapToGlobal(QPoint(width() / 2, height() / 2)));
		updateLookAt();
	}
}

bool Canvas::eventFilter(QObject * obj, QEvent * event) {
	if (event->type() == QEvent::KeyPress) {
		switch (static_cast<QKeyEvent*>(event)->key()) {
			case Qt::Key::Key_Up:
			case Qt::Key::Key_W:
				cameraPos += lookPoint * 0.2f;
				break;
			case Qt::Key::Key_Down:
			case Qt::Key::Key_S:
				cameraPos -= lookPoint * 0.2f;
				break;
			case Qt::Key::Key_Left:
			case Qt::Key::Key_A:
				cameraPos -= lookPoint * upVector * 0.2f;
				break;
			case Qt::Key::Key_Right:
			case Qt::Key::Key_D:
				cameraPos += lookPoint * upVector * 0.2f;
				break;
			case Qt::Key_Shift:
			case Qt::Key::Key_E:
				cameraPos += upVector * 0.2f;
				break;
			case Qt::Key_Control:
			case Qt::Key::Key_Q:
				cameraPos -= upVector * 0.2f;
				break;
		}
		updateLookAt();
		return true;
	} else if (event->type() == QEvent::Wheel && isMouseLocked) {
		if (static_cast<QWheelEvent*>(event)->delta() < 0)
			cameraPos -= lookPoint * 0.1f;
		else
			cameraPos += lookPoint * 0.1f;
		updateLookAt();
		return true;
	}
	return QObject::eventFilter(obj, event);
}

GLuint* Canvas::generateBuffers(size_t n) {
	GLuint* b = new GLuint[n];
	glGenBuffers(n, b);
	return b;
}

void Canvas::linkPrograms() {
	GLuint f = readShader(GL_FRAGMENT_SHADER, "FragmentShader.glsl");
	GLuint v = readShader(GL_VERTEX_SHADER, "VertexShader.glsl");

	program = makeProgram({f, v});
	locs = getShaderUniformLocs();
}
GLuint Canvas::makeProgram(std::initializer_list<GLuint> shaders) {
	GLuint program = glCreateProgram();

	for (auto &shader : shaders)
		glAttachShader(program, shader);

	glLinkProgram(program);

	GLint isLinked;
	glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
	if (!isLinked) {
		GLsizei len;
		glGetShaderiv(program, GL_INFO_LOG_LENGTH, &len);

		GLchar* log = new GLchar[len + 1];
		glGetProgramInfoLog(program, len, &len, log);
		std::string t = std::string(log);
		delete[] log;

		throw CompilationOrLinkingError();
	}

	return program;
}
GLuint Canvas::readShader(GLenum type, std::string fileName) {
	GLuint shader = glCreateShader(type);
	const GLchar* temp[1];
	std::string s;
	s = readFile(fileName);
	temp[0] = s.c_str();
	glShaderSource(shader, 1, temp, 0);
	glCompileShader(shader);

	GLint isCompiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);

	if (!isCompiled) {
		GLsizei len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

		GLchar* log = new GLchar[len + 1];
		glGetShaderInfoLog(shader, len, &len, log);
		std::string t = std::string(log);
		delete[] log;

		throw CompilationOrLinkingError();
	}

	return shader;
}
std::string Canvas::readFile(std::string fileName) {
	return std::string(
		std::istreambuf_iterator<char>(std::ifstream(fileName)),
		std::istreambuf_iterator<char>()
	);
}

UniformLocations Canvas::getShaderUniformLocs() {
	UniformLocations t_locs;
	t_locs.projectionMatrixLoc = glGetUniformLocation(program, "projectionMatrix");
	t_locs.lookAtMatrixLoc = glGetUniformLocation(program, "lookAtMatrix");
	t_locs.cameraLoc = glGetUniformLocation(program, "camera");
	t_locs.rotationSceneMatrixLoc = glGetUniformLocation(program, "rotationSceneMatrix");
	t_locs.translationMatrixLoc = glGetUniformLocation(program, "translationMatrix");
	t_locs.rotationElementMatrixLoc = glGetUniformLocation(program, "rotationElementMatrix");
	t_locs.scalingElementMatrixLoc = glGetUniformLocation(program, "scalingElementMatrix");
	t_locs.scalingSceneMatrixLoc = glGetUniformLocation(program, "scalingSceneMatrix");
	t_locs.drawingColorLoc = glGetUniformLocation(program, "drawingColor");
	return t_locs;
}

void Canvas::sendData() {
	delete[] buffers;
	if (useNormals) {
		buffers = generateBuffers(elementN->getElementsNumber());
		sendElement(elementN);
	} else {
		buffers = generateBuffers(element->getElementsNumber());
		sendElement(element);
	}
}

void Canvas::sendElement(SimpleElement *el, GLuint buffer) {
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	float* res = new float[el->getSize()];
	size_t i = 0;
	for (Point p : **el) {
		res[i++] = p.x();
		res[i++] = p.y();
		res[i++] = p.z();
	}

	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * el->getSize(),
				 res, GL_DYNAMIC_DRAW);
	delete[] res;
}

void Canvas::sendElement(SimpleElement *el) {
	sendElement(el, buffers[0]);
}

void Canvas::sendElement(ComplexElement *el) {
	size_t i = 0;
	for (auto se : **el)
		sendElement(se, buffers[i++]);
}

void Canvas::drawElement(SimpleElement *el, GLuint buffer, float x, float y) {
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	QMatrix4x4 matrix;
	matrix.translate(x, y);
	if (isMovementHolderInserted) {
		auto p = insertedMovementHolder->getPos();
		matrix.translate(p.x(), p.y(), p.z());
	}
	glUniformMatrix4fv(locs.translationMatrixLoc, 1, GL_FALSE, matrix.data());
	glDrawArrays(el->getConnection(), 0, el->getNumber());
}
void Canvas::drawElement(SimpleElement *el, float x, float y) {
	drawElement(el, buffers[0], x, y);
}

void Canvas::drawElement(ComplexElement * el, float x, float y) {
	size_t i = 0;
	if (el) for (auto se : **el)
		drawElement(se, buffers[i++], x, y);
}

void Canvas::drawElement(ComplexElement * el, CoordinatesHolder c) {
	for (auto p : *c)
		drawElement(el, p.x(), p.y());
}

void Canvas::drawElement(ComplexNormalElement * el, float x, float y) {
	size_t i = 0;
	if (el) for (auto se : **el) {
		auto sne = (SimpleNormalElement*) se;
		if (checkCullFacingWithRotation(sne->getPlane()))
			drawElement(sne, buffers[i], x, y);
		i++;
	}
}

void Canvas::drawElement(ComplexNormalElement * el, CoordinatesHolder c) {
	for (auto p : *c)
		drawElement(el, p.x(), p.y());
}

void Canvas::insertMovementHolder(AbstractMovementHolder * mh) {
	insertedMovementHolder = mh;
	isMovementHolderInserted = true;
}

void Canvas::removeMovementHolder() {
	insertedMovementHolder = nullptr;
	isMovementHolderInserted = false;
}

bool Canvas::checkCullFacing(Plane & p) {
	return (-cameraPos ^ (lookMatrix * p).getNormal()) < 0.f;
}

bool Canvas::checkCullFacingWithRotation(Plane & p) {
	return checkCullFacing(rotation * p);
}

QVector3D operator!(const Point& p) {
	return QVector3D(p.x(), p.y(), p.z());
}

void Canvas::createSquareCircle() {
	removeMovementHolder();
	useNormals = false;
	if (element) delete element;
	element = new SquareCircle(0.f, 0.f, 1.f, 90);
	sendData();
	update();
}
void Canvas::createLab2Primitive(float a, float b, float r, size_t n) {
	removeMovementHolder();
	useNormals = false;
	if (element) delete element;
	element = new Lab2Primitive(a, b, r, n);
	sendData();
	update();
}

void Canvas::createLab3Primitive(size_t n) {
	removeMovementHolder();
	useNormals = false;
	if (element) delete element;
	element = new Lab3Primitive(n);
	sendData();
	update();
}

void Canvas::createLab4LinearPrimitive(float a, float b, size_t n, bool x, bool y, bool xa, bool ya) {
	removeMovementHolder();
	useNormals = false;
	if (element) delete element;
	element = new Lab4LinearPrimitive(a, b, n, x, y, xa, ya);
	sendData();
	update();
}

void Canvas::createLab4ColumnPrimitive(float a, float b, size_t n, bool x, bool y, bool xa, bool ya) {
	removeMovementHolder();
	useNormals = false;
	if (element) delete element;
	element = new Lab4ColumnPrimitive(a, b, n, x, y, xa, ya);
	sendData();
	update();
}

void Canvas::createLab4SectorPrimitive(float a, float b, size_t n, bool x, bool y, bool xa, bool ya) {
	removeMovementHolder();
	useNormals = false;
	if (element) delete element;
	element = new Lab4SectorPrimitive(a, b, n, x, y, xa, ya);
	sendData();
	update();
}

void Canvas::createLab5Primitive(AbstractMovementHolder* mh) {
	createLab3Primitive(15);
	useNormals = false;
	insertMovementHolder(mh);
}

void Canvas::createLab6Primitive() {
	removeMovementHolder();
	useNormals = true;
	if (elementN) delete elementN;
	elementN = new Lab6Primitive();
	sendData();
	update();
}

void Canvas::setForegroundR(size_t i) {
	foreground.r = float(i) / 255;
	updateForegroundColor();
}
void Canvas::setForegroundG(size_t i) {
	foreground.g = float(i) / 255;
	updateForegroundColor();
}
void Canvas::setForegroundB(size_t i) {
	foreground.b = float(i) / 255;
	updateForegroundColor();
}
void Canvas::setForegroundA(size_t i) {
	foreground.a = float(i) / 255;
	updateForegroundColor();
}
void Canvas::setBackgroundR(size_t i) {
	background.r = float(i) / 255;
	updateBackgroundColor();
}
void Canvas::setBackgroundG(size_t i) {
	background.g = float(i) / 255;
	updateBackgroundColor();
}
void Canvas::setBackgroundB(size_t i) {
	background.b = float(i) / 255;
	updateBackgroundColor();
}
void Canvas::setBackgroundA(size_t i) {
	background.a = float(i) / 255;
	updateBackgroundColor();
}

void Canvas::setSize(size_t i) {
	QMatrix4x4 matrix;
	matrix.scale(float(i) / 1000);
	glUseProgram(program);
	glUniformMatrix4fv(locs.scalingElementMatrixLoc, 1, GL_FALSE, matrix.data());
	update();
}
void Canvas::setScale(size_t i) {
	QMatrix4x4 matrix;
	matrix.scale(float(i) / 2000);
	glUseProgram(program);
	glUniformMatrix4fv(locs.scalingSceneMatrixLoc, 1, GL_FALSE, matrix.data());
	update();
}
void Canvas::setElementAngle(size_t x, size_t y, size_t z) {
	QMatrix4x4 matrix;
	matrix.rotate(x, 1.f, 0.f, 0.f);
	matrix.rotate(y, 0.f, 1.f, 0.f);
	matrix.rotate(z, 0.f, 0.f, 1.f);
	rotation = matrix;
	glUseProgram(program);
	glUniformMatrix4fv(locs.rotationElementMatrixLoc, 1, GL_FALSE, matrix.data());
	update();
}
void Canvas::setSceneAngle(size_t i) {
	QMatrix4x4 matrix;
	matrix.rotate(i, 0.f, 0.f, 1.f);
	glUseProgram(program);
	glUniformMatrix4fv(locs.rotationSceneMatrixLoc, 1, GL_FALSE, matrix.data());
	update();
}
void Canvas::setLineWidth(size_t i) {
	glLineWidth(float(i) / 100);
	update();
}

void Canvas::setNumber(size_t i) {
	coordinates.setN(i);
	update();
}
void Canvas::randomSlot() {
	coordinates.setRandom();
	update();
}
void Canvas::circleSlot() {
	coordinates.setCircle();
	update();
}
void Canvas::centerSlot() {
	coordinates.setCenter();
	update();
}

void Canvas::resetCamera() {
	cameraPos = Point(0, 0, -2);
	lookPoint = Point(0, 0, 1);
	upVector = Point(0, 1, 0);
	update();
}

void Canvas::lookAtNull() {
	lookPoint = -cameraPos;
	update();
}

void Canvas::updateLookAt() {
	QMatrix4x4 m; m.lookAt(!cameraPos, !(lookPoint + cameraPos), !upVector);
	lookMatrix = m;

	glUseProgram(program);
	glUniformMatrix4fv(locs.lookAtMatrixLoc, 1, GL_FALSE, m.data());
	glUniform3f(locs.cameraLoc, cameraPos.x(), cameraPos.y(), cameraPos.z());

	update();
}
void Canvas::updateForegroundColor() {
	glUseProgram(program);
	glUniform4f(locs.drawingColorLoc,
				foreground.r, foreground.g, foreground.b, foreground.a);
	update();
}
void Canvas::updateBackgroundColor() {
	glUseProgram(program);
	glClearColor(background.r, background.g, background.b, background.a);
	update();
}

void Canvas::update() {
	QOpenGLWidget::update();
}

GLenum _enumSwitch(VertexConnectionType e) {
	switch (e) {
		case VertexConnectionType::Points: return GL_POINTS;
		case VertexConnectionType::Lines: return GL_LINES;
		case VertexConnectionType::LineStrip: return GL_LINE_STRIP;
		case VertexConnectionType::LineLoop: return GL_LINE_LOOP;
		case VertexConnectionType::Triangles: return GL_TRIANGLES;
		case VertexConnectionType::TriangleStrip: return GL_TRIANGLE_STRIP;
		case VertexConnectionType::TriangleFan: return GL_TRIANGLE_FAN;
		case VertexConnectionType::Quads: return GL_QUADS;
		case VertexConnectionType::QuadStrip: return GL_QUAD_STRIP;
		case VertexConnectionType::Polygon: return GL_POLYGON;
		default: return 0;
	}
}
