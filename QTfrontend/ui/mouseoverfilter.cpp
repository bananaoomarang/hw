
#include <QEvent>
#include <QWidget>
#include <QStackedLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>

#include "mouseoverfilter.h"
#include "ui/page/AbstractPage.h"
#include "ui_hwform.h"
#include "hwform.h"
#include "gameuiconfig.h"
#include "DataManager.h"
#include "SDLInteraction.h"

MouseOverFilter::MouseOverFilter(QObject *parent) :
    QObject(parent)
{
}

bool MouseOverFilter::eventFilter( QObject *dist, QEvent *event )
{
    if (event->type() == QEvent::Enter)
    {
        QWidget * widget = dynamic_cast<QWidget*>(dist);

        abstractpage = qobject_cast<AbstractPage*>(ui->Pages->currentWidget());

        if (widget->whatsThis() != NULL)
            abstractpage->setButtonDescription(widget->whatsThis());
        else if (widget->toolTip() != NULL)
            abstractpage->setButtonDescription(widget->toolTip());

        // play a sound when mouse hovers certain ui elements
        QPushButton * button = dynamic_cast<QPushButton*>(dist);
        QLineEdit * textfield = dynamic_cast<QLineEdit*>(dist);
        QCheckBox * checkbox = dynamic_cast<QCheckBox*>(dist);
        QComboBox * droplist = dynamic_cast<QComboBox*>(dist);
        QSlider * slider = dynamic_cast<QSlider*>(dist);
        QTabWidget * tab = dynamic_cast<QTabWidget*>(dist);
        if (HWForm::config->isFrontendSoundEnabled() && (button || textfield || checkbox || droplist || slider || tab))
        {
            DataManager & dataMgr = DataManager::instance();
            SDLInteraction::instance().playSoundFile(dataMgr.findFileForRead("Sounds/steps.ogg"));
        }

        return true;
    }
    else if (event->type() == QEvent::Leave)
    {
        abstractpage = qobject_cast<AbstractPage*>(ui->Pages->currentWidget());

        if (abstractpage->getDefautDescription() != NULL)
        {
            abstractpage->setButtonDescription( * abstractpage->getDefautDescription());
        }
        else
            abstractpage->setButtonDescription("");
    }

    return false;
}

void MouseOverFilter::setUi(Ui_HWForm *uiForm)
{
    ui = uiForm;
}