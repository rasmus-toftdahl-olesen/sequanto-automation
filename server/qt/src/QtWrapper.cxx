#include "config.h"

#include <sequanto/ui.h>
#include <sequanto/log.h>
#include <sequanto/QtWidgetNode.h>
#include <sequanto/QtWrapper.h>
#include <sequanto/QtWidgetAutomationEventFilter.h>
#include <sequanto/QtApplicationAutomationEventFilter.h>
#include <sequanto/QtAutomationGetPropertyEvent.h>
#include <sequanto/QtAutomationMoveEvent.h>
#include <sequanto/QtAutomationResizeEvent.h>
#include <sequanto/QtAutomationMouseMoveEvent.h>
#include <sequanto/QtAutomationMouseClickEvent.h>
#include <sequanto/QtUnnamedObjectStore.h>
#include <sequanto/QtUiTypeProperty.h>
#include <sequanto/QtStringProperty.h>
#include <sequanto/QtIntegerProperty.h>
#include <sequanto/QtBooleanProperty.h>
#include <sequanto/QtColorProperty.h>
#include <sequanto/QtMouseClickMethod.h>
#include <sequanto/QtMouseMoveMethod.h>
#include <sequanto/QtActiveWindowProperty.h>
#include <sequanto/QtScrollBarProperty.h>
#include <sequanto/QtAutoGeneratedNameProperty.h>
#include <sequanto/QtNativeIdProperty.h>
#include <sequanto/QtScreenPositionProperty.h>
#include <sequanto/QtGlobalPositionProperty.h>
#include <sequanto/QtMethod2Parameters.h>
#include <sequanto/QtMoveMethod.h>
#include <sequanto/QtResizeMethod.h>
#include <sequanto/QtInputMethod.h>
#include <sequanto/QtTableRowsProperty.h>
#include <sequanto/QtTableColumnsProperty.h>
#include <sequanto/QtTableCellTextMethod.h>
#include <sequanto/QtTableRowHeightMethod.h>
#include <sequanto/QtTableColumnWidthMethod.h>
#include <sequanto/QtTableUpdateProperty.h>
#include <sequanto/QtTableRowHeaderWidthProperty.h>
#include <sequanto/QtTableColumnHeaderHeightProperty.h>
#include <sequanto/QtTableRowHeaderMethod.h>
#include <sequanto/QtTableColumnHeaderMethod.h>
#include <sequanto/QtListLinesProperty.h>
#include <sequanto/QtListUpdateProperty.h>
#include <sequanto/QtListLineHeightMethod.h>
#include <sequanto/QtListLineTextMethod.h>
#include <sequanto/QtTabsActiveTabProperty.h>
#include <sequanto/QtTabsTabPositionProperty.h>
#include <sequanto/QtTabsTabHeightProperty.h>
#include <sequanto/QtTabsTabNameMethod.h>
#include <sequanto/QtTabsTabWidthMethod.h>
#include <sequanto/QtStatsProperties.h>
#include <sequanto/QtDebugging.h>
#include <sequanto/QtLogging.h>

#ifdef SQ_QT_USE_CACHE
#include <sequanto/QtCachedProperty.h>
#endif

#include <cassert>
#include <vector>
#include <stdexcept>
#include <iostream>

using namespace sequanto::automation;

QtWrapper::QtWrapper()
{
}

QVariant QtWrapper::GetPropertyValue ( QObject * _object, const std::string & _propertyName )
{
  if ( _object->thread() == QThread::currentThread() )
  {
    if ( _object == QApplication::activeWindow() && _propertyName == SQ_UI_COMMON_BASE_VISIBLE )
    {
      //qDebug() << "QtWrapper::GetPropertyValue: The current window is always visible.";
      return true;
    }
    else if ( _propertyName == QtWrapper::screen_pos() )
    {
       QWidget * widget = qobject_cast<QWidget*>(_object);
       return widget->geometry().topLeft();
    }
    else if ( _propertyName == QtWrapper::global_pos() )
    {
       QWidget * widget = qobject_cast<QWidget*>(_object);
       QWidget * window = widget->window();
       
       QPoint pos = widget->mapToGlobal(QPoint(0,0));
       QPoint windowTopLeft ( window->geometry().topLeft() );
	
       pos -= windowTopLeft;

       return pos;
    }
    else
    {
      return _object->property(_propertyName.c_str());
    }
  }
  else
  {
     QtAutomationGetPropertyEvent * event = new QtAutomationGetPropertyEvent(_propertyName.c_str());
     QVariant value = event->wait ( _object );
     return value;
  }
}

const std::string & QtWrapper::screen_pos ()
{
  static std::string SCREEN_POS ( "__sq_screen" );
  return SCREEN_POS;
}

const std::string & QtWrapper::global_pos ()
{
  static std::string GLOBAL_POS ( "__sq_global" );
  return GLOBAL_POS;
}


std::string QtWrapper::ToString ( const QString & _string )
{
   QByteArray value ( _string.toUtf8() );
   return std::string ( value.constData(), value.length() );
}

std::string QtWrapper::GetObjectName ( QObject * _object )
{
   QString objectName ( _object->objectName() );
   if ( objectName.isEmpty() )
	{
      return QtUnnamedObjectStore::GetName ( _object );
	}
   QByteArray value ( _object->objectName().toUtf8() );
   if ( !Node::IsValidName(value.constData(), value.length()) )
   {
      return QtUnnamedObjectStore::GetName ( _object );
   }
	else
	{
      return std::string(value.constData(), value.length() );
	}
}

void QtWrapper::Wrap ( ListNode * _root, QObject * _object )
{
   std::string name ( GetObjectName(_object) );
   _root->AddChild ( new ConstantStringNode("name", name ) );
   _root->AddChild ( new ConstantStringNode("className", _object->metaObject()->className() ) );

   for ( int i = 0; i < _object->metaObject()->propertyCount(); i++ )
   {
#if QT_VERSION >= 0x040500
      if ( _object->metaObject()->property(i).hasNotifySignal() )
      {
         std::string text ( "The " );
         text += _object->metaObject()->property(i).name();
         text += "property has a notify signal";
      }
#endif
      if ( !_root->HasChild ( _object->metaObject()->property(i).name() ) )
      {
         switch ( _object->metaObject()->property(i).type() )
         {
         case QVariant::String:
            _root->AddChild ( new QtStringProperty(_object->metaObject()->property(i).name(), _object ) );
            break;

         case QVariant::Int:
            _root->AddChild ( new QtIntegerProperty(_object->metaObject()->property(i).name(), _object ) );
            break;

         case QVariant::Bool:
            _root->AddChild ( new QtBooleanProperty(_object->metaObject()->property(i).name(), _object ) );
            break;

         default:
            break;
         }
      }
   }

   for ( int i = 0; i < _object->metaObject()->methodCount(); i++ )
   {
      int methodIndex = _object->metaObject()->methodOffset() + i;
      QMetaMethod method ( _object->metaObject()->method( methodIndex ) );
      if ( method.methodType() == QMetaMethod::Method )
      {
         if ( method.signature() != 0 )
         {
            if ( QtMethod2Parameters::Valid(method) )
            {
               if ( method.parameterTypes().size() == 2 )
               {
                  std::string methodName ( method.signature() );
                  size_t i = methodName.find("(");
                  methodName = methodName.substr(0, i);
                  if ( !_root->HasChild(methodName) )
                  {
                     _root->AddChild ( new QtMethod2Parameters(methodName, _object, methodIndex) );
                  }
               }
            }
         }
      }
   }

   QObjectList list ( _object->children() );
   if ( !list.empty() )
   {
      ListNode * children = new ListNode ( "children" );

      for ( int i = 0; i < list.count(); i++ )
      {
         QObject * childObject = list.at ( i );

         std::string childName ( GetObjectName(childObject) );
         if ( !childName.empty() )
         {
            ListNode * child = new ListNode ( childName );
            Wrap ( child, childObject );
            children->AddChild ( child );
         }
      }
      _root->AddChild ( children );
   }
}

void QtWrapper::WrapUi ( QtWidgetNode * _root, QWidget * _widget )
{
   bool wrapChildren = true;
   if ( QtWrapper::IsWindow(_widget) )
   {
      assert ( _widget == _widget->window() );
      
      _root->AddChild ( new QtScreenXProperty( _widget ) );
      _root->AddChild ( new QtScreenYProperty( _widget ) );
      _root->AddChild ( new QtColorProperty(SQ_UI_WINDOW_BACKGROUND_COLOR, _widget, QPalette::Background) );
      _root->AddChild ( new QtMoveMethod(_widget ) );
      _root->AddChild ( new QtResizeMethod(_widget ) );
   }
   else if ( _widget->inherits ( QCheckBox::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_CHECK_BOX) );
      _root->AddChild ( new QtBooleanProperty(SQ_UI_CHECK_BOX_CHECKED, _widget) );
      _root->AddChild ( new QtStringProperty(SQ_UI_CHECK_BOX_TEXT, _widget) );
   }
   else if ( _widget->inherits ( QRadioButton::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_RADIO_BUTTON) );
      _root->AddChild ( new QtBooleanProperty(SQ_UI_RADIO_BUTTON_CHECKED, _widget) );
      _root->AddChild ( new QtStringProperty(SQ_UI_RADIO_BUTTON_TEXT, _widget) );
   }
   else if ( _widget->inherits ( QLineEdit::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_TEXT_BOX) );
      _root->AddChild ( new QtStringProperty(SQ_UI_TEXT_BOX_TEXT, _widget) );
   }
   else if ( _widget->inherits ( QPlainTextEdit::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_TEXT_BOX) );
      _root->AddChild ( new QtStringPropertyWithAlternateName(SQ_UI_TEXT_BOX_TEXT, _widget, "plainText") );
   }
   else if ( _widget->inherits ( QTextEdit::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_TEXT_BOX) );
      _root->AddChild ( new QtStringPropertyWithAlternateName(SQ_UI_TEXT_BOX_TEXT, _widget, "plainText", "html") );
   }
   else if ( _widget->inherits ( QAbstractButton::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_BUTTON) );
      _root->AddChild ( new QtStringProperty(SQ_UI_BUTTON_TEXT, _widget) );
   }
   else if ( _widget->inherits ( QMenuBar::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_MENU_BAR) );
   }
   else if ( _widget->inherits ( QMenu::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_MENU_ITEM) );
   }
   else if ( _widget->inherits ( QStatusBar::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_STATUS_BAR) );
   }
   else if ( _widget->inherits ( QLayout::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_CONTAINER) );
   }
   else if ( _widget->inherits ( QLabel::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_LABEL) );
      _root->AddChild ( new QtStringProperty(SQ_UI_LABEL_TEXT, _widget) );
   }
   else if ( _widget->inherits( QAbstractSlider::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_SLIDER) );
      _root->AddChild ( new QtScrollBarProperty(SQ_UI_SLIDER_VALUE, _widget) );
      _root->AddChild ( new QtIntegerProperty(SQ_UI_SLIDER_MINIMUM, _widget) );
      _root->AddChild ( new QtIntegerProperty(SQ_UI_SLIDER_MAXIMUM, _widget) );
   }
   else if ( _widget->inherits ( QScrollArea::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_SCROLL_AREA) );
   }
   else if ( _widget->inherits( QTableWidget::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_TABLE) );
      _root->AddChild ( new QtTableRowsProperty() );
      _root->AddChild ( new QtTableColumnsProperty() );
      _root->AddChild ( new QtTableCellTextMethod() );
      _root->AddChild ( new QtTableRowHeightMethod() );
      _root->AddChild ( new QtTableColumnWidthMethod() );
      _root->AddChild ( new QtTableUpdateProperty() );
      _root->AddChild ( new QtTableRowHeaderWidthProperty() );
      _root->AddChild ( new QtTableColumnHeaderHeightProperty() );
      _root->AddChild ( new QtTableRowHeaderMethod() );
      _root->AddChild ( new QtTableColumnHeaderMethod() );
      
      wrapChildren = false;
   }
   else if ( _widget->inherits( QListWidget::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_LIST) );
      _root->AddChild ( new QtListLinesProperty() );
      _root->AddChild ( new QtListUpdateProperty() );
      _root->AddChild ( new QtListLineHeightMethod() );
      _root->AddChild ( new QtListLineTextMethod() );
   }
   else if ( _widget->inherits( QTabWidget::staticMetaObject.className() ) )
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_TABS) );
      _root->AddChild ( new QtIntegerProperty(SQ_UI_TABS_COUNT, _widget ) );
      _root->AddChild ( new QtTabsActiveTabProperty() );
      _root->AddChild ( new QtTabsTabPositionProperty() );
      _root->AddChild ( new QtTabsTabHeightProperty() );
      _root->AddChild ( new QtTabsTabNameMethod() );
      _root->AddChild ( new QtTabsTabWidthMethod() );
   }
   else
   {
      _root->AddChild ( new QtUiTypeProperty(SQ_WIDGET_TYPE_WIDGET) );
   }
   _root->AddChild ( new ConstantStringNode ( SQ_UI_COMMON_BASE_NATIVE_TYPE, _widget->metaObject()->className() ) );
   _root->AddChild ( new QtAutoGeneratedNameProperty() );
   _root->AddChild ( new QtNativeIdProperty() );
   
#ifdef SQ_QT_USE_CACHE
   _root->AddChild ( new QtCachedIntegerProperty( SQ_UI_WIDGET_X, _widget, QtCacheItem::GLOBAL_X ) );
   _root->AddChild ( new QtCachedIntegerProperty( SQ_UI_WIDGET_Y, _widget, QtCacheItem::GLOBAL_Y ) );
   
   _root->AddChild ( new QtCachedIntegerProperty( SQ_UI_COMMON_BASE_WIDTH, _widget, QtCacheItem::WIDTH ) );
   _root->AddChild ( new QtCachedIntegerProperty( SQ_UI_COMMON_BASE_HEIGHT, _widget, QtCacheItem::HEIGHT ) );

   _root->AddChild ( new QtCachedBooleanProperty( SQ_UI_COMMON_BASE_ENABLED, _widget, QtCacheItem::ENABLED ) );
   _root->AddChild ( new QtCachedBooleanProperty( SQ_UI_COMMON_BASE_VISIBLE, _widget, QtCacheItem::VISIBLE ) );
#else
   _root->AddChild ( new QtGlobalXProperty( _widget ) );
   _root->AddChild ( new QtGlobalYProperty( _widget ) );
   
   _root->AddChild ( new QtIntegerProperty( SQ_UI_COMMON_BASE_WIDTH, _widget ) );
   _root->AddChild ( new QtIntegerProperty( SQ_UI_COMMON_BASE_HEIGHT, _widget ) );

   _root->AddChild ( new QtBooleanProperty( SQ_UI_COMMON_BASE_ENABLED, _widget ) );
   _root->AddChild ( new QtBooleanProperty( SQ_UI_COMMON_BASE_VISIBLE, _widget ) );
#endif
   
   _root->AddChild ( new QtInputMethod() );
   
   _root->AddChild ( new ListNode(SQ_UI_COMMON_BASE_CHILDREN) );
   
   if ( wrapChildren )
   {
      QObjectList list ( _widget->children() );
      if ( !list.empty() )
      {
         for ( int i = 0; i < list.count(); i++ )
         {
            QObject * childObject = list.at ( i );
            if ( childObject->isWidgetType() )
            {
               QWidget * childWidget = qobject_cast<QWidget*>(childObject);
               if ( !IsWindow(childWidget) )
               {
                   QtWidgetNode::AddChildWidgetResult result =_root->AddChildWidget ( childWidget, false );
                   
                   if ( result == QtWidgetNode::ALREADY_EXISTS_BUT_REMOVED_SINCE_IT_IS_NOT_VISIBLE )
                   {
                       _root->AddChildWidget ( childWidget, false );
                   }
               }
            }
         }
      }
   }
}

ListNode * s_applicationRoot;

ListNode * QtWrapper::ApplicationRoot ()
{
   return s_applicationRoot;
}

void QtWrapper::WrapApplication ( ListNode * _root )
{
    s_applicationRoot = _root;
    
   ListNode * windows = new ListNode ( SQ_UI_ROOT_WINDOWS );
   _root->AddChild ( windows );
   QtActiveWindowProperty * activeWindow = new QtActiveWindowProperty();
   _root->AddChild ( activeWindow );

   UpdateWindows ( windows, activeWindow );

   QDesktopWidget * desktop = QApplication::desktop();
   ListNode * screen = new ListNode ( SQ_UI_ROOT_SCREEN );
   _root->AddChild ( screen );
   screen->AddChild ( new ConstantIntegerNode(SQ_UI_SCREEN_WIDTH, desktop->screenGeometry ().width() ) );
   screen->AddChild ( new ConstantIntegerNode(SQ_UI_SCREEN_HEIGHT, desktop->screenGeometry ().height() ) );
   
   ListNode * mouse = new ListNode ( SQ_UI_ROOT_MOUSE );
   _root->AddChild ( mouse );
   mouse->AddChild ( new QtMouseMoveMethod() );
   mouse->AddChild ( new QtMouseClickMethod() );

   QApplication::instance()->installEventFilter ( new QtApplicationAutomationEventFilter(windows, activeWindow, QApplication::instance()) );

   ListNode * stats = new ListNode ( "stats" );
   _root->AddChild ( stats );
   stats->AddChild ( new QtStatsGetPropertyEventsHandled() );
   stats->AddChild ( new QtStatsGetPropertyAverageDeliveryTime() );
   stats->AddChild ( new QtStatsCacheHits() );
   stats->AddChild ( new QtStatsCacheMisses() );
   
   ListNode * logging = new ListNode ( "logging" );
   _root->AddChild ( logging );
   logging->AddChild ( new QtLoggingEnabledProperty() );
   logging->AddChild ( new QtLoggingFilenameProperty() );

   ListNode * debugging = new ListNode ( "debugging" );
   _root->AddChild ( debugging );
   debugging->AddChild ( new QtDebuggingVerifyIntegrityMethod() );
}

bool QtWrapper::IsWindow ( QWidget * _widget )
{
   if ( _widget->isWindow() && _widget == _widget->window() &&
        ( _widget->inherits(QMainWindow::staticMetaObject.className()) ||
          _widget->inherits(QDialog::staticMetaObject.className()) ||
          _widget->isVisible() ) )
   {
	  return true;
   }
   else
   {
      return false;
   }
}

bool QtWrapper::UpdateWindows()
{
   ListNode * windowsNode = dynamic_cast<ListNode*>(s_applicationRoot->FindChild ( SQ_UI_ROOT_WINDOWS ));
   QtActiveWindowProperty * activeWindowNode = dynamic_cast<QtActiveWindowProperty*>(s_applicationRoot->FindChild ( SQ_UI_ROOT_ACTIVE_WINDOW ));
   if ( windowsNode != NULL && activeWindowNode != NULL )
   {
      return UpdateWindows ( windowsNode, activeWindowNode );
   }
   else
   {
      return false;
   }
}

bool QtWrapper::UpdateWindows( ListNode * _windows, QtActiveWindowProperty * _activeWindowNode )
{
    //QChar zero ( '0' );
    //qDebug() << "UpdateWindows()";
    //qDebug() << QString("   Currently, active window is: %1").arg((ulong) QApplication::activeWindow(), 8, 16, zero);
   
   bool changed = false;
   std::map<std::string, QWidget*> windows;
   std::map<std::string, QWidget*>::iterator windowsIt;
   foreach ( QWidget * widget, QApplication::allWidgets() )
   {
      if ( IsWindow(widget) )
      {
         std::string objectName ( GetObjectName(widget) );
         if ( !_windows->HasChild ( objectName ))
         {
            if ( QtUnnamedObjectStore::IsKnown ( widget ) )
            {
               std::string unnamedObjectName ( QtUnnamedObjectStore::GetName(widget) );
               if ( _windows->HasChild(unnamedObjectName) )
               {
                  _windows->RemoveChild ( unnamedObjectName );
                  QtUnnamedObjectStore::Deleted ( widget );
               }
            }
            
            changed = true;
         }
         
         windowsIt = windows.find(objectName);
         if ( windowsIt == windows.end() )
         {
             //qDebug() << QString("Found first window named %1 (%2)").arg(objectName.c_str()).arg((ulong) widget, 8, 16, zero);
             windows.insert ( std::make_pair(objectName, widget) );
         }
         else
         {
             //qDebug() << "Found duplicate of " << objectName.c_str();
             
             // Two windows with the same name found, we must decide which to keep
             
             // If one of the windows is the active window, we just use that.
             if ( widget == QApplication::activeWindow() )
             {
                 //qDebug() << QString("Choosing the new window since it is the active window (%1).").arg((ulong) widget, 8, 16, zero);
                 windows.erase ( windowsIt );
                 windows.insert ( std::make_pair(objectName, widget) );
             }
             else if ( windowsIt->second == QApplication::activeWindow() )
             {
                 //qDebug() << QString("Keeping the already existing one because it is the active window (%1).").arg((ulong) windowsIt->second, 8, 16, zero);
             }
             else 
             {
                 // If the newest window is visible we always keep that one
                 if ( widget->isVisible() )
                 {
                     //qDebug() << QString("Using the new one (%1) since it is visible.").arg((ulong) widget, 8, 16, zero);
                     windows.erase ( windowsIt );
                     windows.insert ( std::make_pair(objectName, widget) );
                 }
                 else
                 {
                     if ( !windowsIt->second->isVisible() )
                     {
                         // If the old widget is not visible either, we keep the newest one
                         //qDebug() << QString("Using the new one (%1) since the old one (%2) is not visible.").arg((ulong) windowsIt->second, 8, 16, zero);
                         windows.erase ( windowsIt );
                         windows.insert ( std::make_pair(objectName, widget) );
                     }
                 }
             }
         }
      }
   }
   
   /*
   qDebug() << "   Found windows:";
   for ( windowsIt = windows.begin(); windowsIt != windows.end(); windowsIt ++ )
   {
       qDebug() << QString("   %1 visible? %2 (%3)").arg(windowsIt->first.c_str()).arg(windowsIt->second->isVisible()).arg((ulong) windowsIt->second, 8, 16, zero);
   }
   */
   
   // Remove old nodes
   std::vector<std::string> toBeRemoved;
   std::vector<std::string> removedBecauseNewWindowAvailable;
   
   ListNode::Iterator * it = _windows->ListChildren();
   for ( ; it->HasNext(); it->Next() )
   {
       windowsIt = windows.find(it->GetCurrent()->GetName());
       if ( windowsIt == windows.end() )
       {
           //qDebug() << "   - Removed (name not found): " << it->GetCurrent()->GetName().c_str();
           toBeRemoved.push_back ( it->GetCurrent()->GetName() );
       }
       else
       {
           if ( dynamic_cast<QtWidgetNode*>(it->GetCurrent())->widget() != windowsIt->second )
           {
               //qDebug() << "   - Removed (window is not the same as widget): " << it->GetCurrent()->GetName().c_str();
               toBeRemoved.push_back ( it->GetCurrent()->GetName() );
               removedBecauseNewWindowAvailable.push_back ( it->GetCurrent()->GetName() );
           }
       }
   }
   delete it;
   it = NULL;
   
   for ( std::vector<std::string>::const_iterator removeIt = toBeRemoved.begin(); removeIt != toBeRemoved.end(); removeIt++ )
   {
      _windows->RemoveChild ( *removeIt );
      changed = true;
   }
   
   for ( windowsIt = windows.begin(); windowsIt != windows.end(); windowsIt++ )
   {
       if ( !_windows->HasChild(windowsIt->first) )
       {
           //qDebug() << "   + Adding: " << windowsIt->first.c_str();
           
           QtWidgetNode * newWindow = new QtWidgetNode ( windowsIt->second );
           WrapUi ( newWindow, windowsIt->second );
           _windows->AddChild ( newWindow );
           newWindow->SendAdd ();
           changed = true;
       }
   }
   
   if ( changed )
   {
	   _activeWindowNode->TrySendUpdate ();
   }
   
   for ( std::vector<std::string>::const_iterator updateIt = removedBecauseNewWindowAvailable.begin(); updateIt != removedBecauseNewWindowAvailable.end(); updateIt++ )
   {
       //qDebug() << "   Sending update for " << updateIt->c_str() << " since it was replaced (new window available, old one not visible)";
       Node * windowChild = _windows->FindChild ( *updateIt );
       if ( windowChild != NULL )
       {
           QtWidgetNode * windowNode = dynamic_cast<QtWidgetNode*> ( windowChild );
           if ( windowNode != NULL )
           {
               //qDebug() << "   Node found.";
               windowNode->SendUpdateForAllChildren();
           }
       }
   }
   return changed;
}


void QtWrapper::ActiveWindowChanged()
{
   ListNode * windowsNode = dynamic_cast<ListNode*>(s_applicationRoot->FindChild ( SQ_UI_ROOT_WINDOWS ));
   QtActiveWindowProperty * activeWindowProperty = dynamic_cast<QtActiveWindowProperty*>(s_applicationRoot->FindChild ( SQ_UI_ROOT_ACTIVE_WINDOW ));
   
   if ( windowsNode != NULL && activeWindowProperty != NULL )
   {
       std::string activeWindowName ( activeWindowProperty->GetValue() );
       
       //qDebug() << "Active window changed to " << activeWindowName.c_str();
       
       QtWidgetNode * activeWindowNode = dynamic_cast<QtWidgetNode*> ( windowsNode->FindChild ( activeWindowName ) );
       
       if ( activeWindowNode != NULL )
       {
           QWidget * window = activeWindowNode->widget();
           
           if ( window != QApplication::activeWindow() )
           {
               //qDebug() << "   The found window is not the active window.";
               UpdateWindows( windowsNode, activeWindowProperty );
           }
       }
   }
}

void QtWrapper::Log ( const QString & _message )
{
   std::string message ( ToString(_message) );
   sq_log ( message.c_str() );
}

QtWrapper::~QtWrapper()
{
}
