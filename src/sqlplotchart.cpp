#include "sqlplotchart.h"
#include "ui_sqlplotchart.h"

#include <QDebug>
#include <QEvent>
#include <QLineEdit>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlResult>
#include <QBuffer>
#include <QProcess>

#include "main.h"
#include "sheetmessagebox.h"

#define SAFE_DELETE(x) if(x){delete x;x = NULL;}

SqlPlotChart::SqlPlotChart(QSqlDatabase *database, QWidget *parent, const QString &defaultName) :
    QDialog(parent),
    ui(new Ui::SqlPlotChart), m_database(database), m_rcode(0), m_rpng(0)
{
    ui->setupUi(this);
    ui->tableComboBox->lineEdit()->setText(defaultName);
    if (database->driverName() == "QSQLITE") {
        QFileInfo info(database->databaseName());
        setWindowTitle(QString("Plot : ") + info.completeBaseName());
    } else {
        setWindowTitle(QString("Plot : ") + database->databaseName());
    }
    ui->sqlFilter->setDatabase(m_database);
    connect(ui->sqlFilter, SIGNAL(returnPressed()), ui->plotButton, SLOT(click()));
    setMaximumSize(size());
    refreshTables();
}

SqlPlotChart::~SqlPlotChart()
{
    delete ui;
    SAFE_DELETE(m_rcode);
    SAFE_DELETE(m_rpng);
}

void SqlPlotChart::setFilter(const QString &filter)
{
    ui->sqlFilter->setPlainText(filter);
}

void SqlPlotChart::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() != QEvent::ActivationChange)
        return;
    //qDebug() << isActiveWindow();
    //if (isActiveWindow())
    //    refreshTables();
}

QString SqlPlotChart::generateRcode(const QString &device)
{
    QList<QVariant> axis[2];
    QString rcode;

    rcode += "library(lattice)\n";

    if (ui->axis1ComboBox->currentText().isEmpty())
        return "";

    QSqlQuery query = m_database->exec(QString("SELECT %1, %2 FROM %3 %4").arg(ui->axis1ComboBox->currentText(),
                                                                               ui->axis2ComboBox->currentText(),
                                                                               ui->tableComboBox->currentText(),
                                                                               ui->sqlFilter->toPlainText().isEmpty() ? "" : QString("WHERE %1").arg(ui->sqlFilter->toPlainText())));
    if (query.lastError().type() != QSqlError::NoError) {
        SheetMessageBox::critical(this, tr("Cannot select table"), query.lastError().text());
        return "";
    }

    if (query.first()) {
        do {
            axis[0] << query.record().value(0);
            axis[1] << query.record().value(1);
        } while (query.next());
    }

    rcode += QString("axis.name <- list(\"%1\", \"%2\")\n").arg(ui->axis1ComboBox->currentText().replace("\"","\\\""), ui->axis2ComboBox->currentText().replace("\"","\\\""));
    rcode += "table.data <- list(";
    for (int i = 0; i < 2; i++) {
        bool first = true;
        rcode += "c(";
        foreach (QVariant value, axis[i]) {
            if (!first) {
                rcode += ",";
            }
            first = false;
            switch(value.type()) {
            case QVariant::Int:
            case QVariant::Double:
            case QVariant::LongLong:
                rcode += value.toString();
                break;
            default:
                qDebug() << value.type();
            case QVariant::String:
                rcode += "\"";
                rcode += value.toString().replace("\"","\\\"");
                rcode += "\"";
                break;
            }
        }
        rcode += ")";
        if (i != 1)
            rcode += ",";
    }
    rcode += ")\n\n";

    rcode += device + "\n";

    switch (ui->chartTypeComboBox->currentIndex()) {
    case 0: { // scatter plot
        switch(ui->pchComboBox->currentIndex()) {
        case 0:
        default:
            rcode += "pch <- 1\n";
             break;
        case 1:
            rcode += "pch <- 20\n";
            break;
        case 2:
            rcode += "pch <- 4\n";
            break;
        case 3:
            rcode += "pch <- 3\n";
            break;
        case 4:
            rcode += "pch <- 2\n";
            break;
        case 5:
            rcode += "pch <- 17\n";
            break;
        case 6:
            rcode += "pch <- 0\n";
            break;
        case 7:
            rcode += "pch <- 15\n";
            break;
        }

        rcode += "alpha <- "+QString::number(ui->alphaSpin->value())+"\n";

        rcode += "cor.result <- cor.test(table.data[[1]], table.data[[2]])\n"
                "xyplot(table.data[[2]] ~ table.data[[1]], alpha=alpha, pch=pch, grid=T, xlab=axis.name[[1]], ylab=axis.name[[2]], main=sprintf(\"Correlation: %f   p-value: %f\", cor.result[[4]], cor.result[[3]]))\n";
        break;
    }
    case 1: { // heatmap
        rcode += "library(gregmisc)\n"
                "cor.result <- cor.test(table.data[[1]], table.data[[2]])\n"
                "h2c <- hist2d(table.data[[1]], table.data[[2]], show=F, same.scale=F, nbins=c(20, 20))\n"
                "print(image(h2c$x, h2c$y, log2(h2c$count + 1), xlab=axis.name[[1]], ylab=axis.name[[2]], main=sprintf(\"Correlation: %f   p-value: %f\", cor.result[[4]], cor.result[[3]])))\n";
        break;
    }
    case 2: { // histogram
        rcode += "nint <- "+QString::number(ui->binSpin->value())+"\n";
        rcode += "histogram(table.data[[1]], nint=nint, xlab=axis.name[[1]])\n";
        break;
    }
    case 3: { // densityplot
        rcode += "densityplot(table.data[[1]], xlab=axis.name[[1]])\n";
        break;
    }
    }

    rcode += "dev.off()\n";
    return rcode;
}

void SqlPlotChart::refreshTables()
{
    QString current = ui->tableComboBox->lineEdit()->text();
    ui->tableComboBox->clear();
    ui->tableComboBox->addItems(m_database->tables(QSql::AllTables));
    ui->tableComboBox->lineEdit()->setText(current);
}

void SqlPlotChart::on_chartTypeComboBox_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
    case 1:
        ui->axis2ComboBox->setEnabled(true);
        break;
    case 2:
    case 3:
        ui->axis2ComboBox->setEnabled(false);
        break;
    }


    ui->alphaSpin->setEnabled(index == 0);
    ui->pchComboBox->setEnabled(index == 0);
    ui->binSpin->setEnabled(index == 2);
}

void SqlPlotChart::on_plotButton_clicked()
{
    SAFE_DELETE(m_rcode);
    SAFE_DELETE(m_rpng);

    m_rcode = new QTemporaryFile(this);
    m_rpng = new QTemporaryFile(this);
    m_rcode->open();
    m_rpng->open();

    QString code = generateRcode(QString("png(\"%1\")").arg(m_rpng->fileName()));

    m_rcode->write(code.toUtf8());
    m_rcode->flush();

    QStringList args;
    args << m_rcode->fileName();
    int retcode = QProcess::execute(tableview_settings->value(PATH_R, suggestRPath()).toString(), args);

    if (retcode) {
        SheetMessageBox::warning(this, tr("Cannot plot"), tr("Something wrong with R. If you are ploting heatmap, please confirm \"gregmisc\" package is installed."));
    }

    QImage img(m_rpng->fileName(), "PNG");
    ui->imageView->setImage(img);
    ui->imageView->repaint();

}

void SqlPlotChart::on_tableComboBox_editTextChanged(const QString &arg1)
{
    qDebug() << "currentIndexChanged " << arg1;
    QSqlRecord record = m_database->record(arg1);
    QStringList columns;
    for (int i = 0; i < record.count(); ++i) {
        columns << record.fieldName(i);
    }
    ui->axis1ComboBox->clear();
    ui->axis2ComboBox->clear();
    ui->axis1ComboBox->addItems(columns);
    ui->axis2ComboBox->addItems(columns);
    ui->sqlFilter->setTable(arg1);
}