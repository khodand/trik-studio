#include <QtGui>

#include "palettetoolbox.h"
#include "realrepoinfo.h"

PaletteToolbox::DraggableElement::DraggableElement(QString className, QWidget *parent/*0*/)
	: QWidget(parent)
{
	m_text = className;
	m_icon = QIcon(":/shapes/" + className + "Class.svg");
	
	QHBoxLayout *layout = new QHBoxLayout(this);
	QLabel *icon = new QLabel(this);
	icon->setFixedSize(24,24);
	icon->setPixmap(m_icon.pixmap(24,24));
	layout->addWidget(icon);

	QLabel *text = new QLabel(this);
	text->setText(m_text);
	layout->addWidget(text);

	setLayout(layout);
}


PaletteToolbox::PaletteToolbox(QWidget *parent)
    : QToolBox(parent)
{
//  setAcceptDrops(true);
	RealRepoInfo info;
	QStringList categories = info.getObjectCategories();
	for (int i = 0; i < categories.size(); i++) {
		QWidget *tab = new QWidget(this);
		QVBoxLayout *layout = new QVBoxLayout(this);
		addItem(tab, categories[i]);

		foreach(QString classname, info.getObjects(i)) {
			DraggableElement *element = new DraggableElement(classname, this);
			layout->addWidget(element);
		}
		
		tab->setLayout(layout);
	}
}

void PaletteToolbox::dragEnterEvent(QDragEnterEvent *event)
{
}

void PaletteToolbox::dropEvent(QDropEvent *event)
{
}

void PaletteToolbox::mousePressEvent(QMouseEvent *event)
{
	QWidget *atMouse = childAt(event->pos());
    DraggableElement *child = dynamic_cast<DraggableElement *>(atMouse->parent());
    if (!child)
		child = dynamic_cast<DraggableElement *>(atMouse);
	if (!child)
		return;

	QByteArray itemData;

	QDataStream stream(&itemData, QIODevice::WriteOnly);
	stream << child->text();

    QMimeData *mimeData = new QMimeData;
	mimeData->setData("application/x-real-uml-data", itemData);
	// mimeData->setText(child->text());
        
    QDrag *drag = new QDrag(this);
	drag->setMimeData(mimeData);
    drag->setPixmap(child->icon().pixmap(32,32));

	if (drag->start(Qt::CopyAction | Qt::MoveAction) == Qt::MoveAction)
        child->close();
    else {
        child->show();
    }
}
