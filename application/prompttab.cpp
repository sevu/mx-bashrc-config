#include "prompttab.h"

#include "searcher.h"
#include <QMap>
#include <QMessageBox>
#include <QProcess>

PromptTab::PromptTab()
    : Tab("Prompt")
{
    DEBUG_ENTER(PromptTab::PromptTab);
    ui = new Ui::PromptTab;
    setWidget(new QWidget);
    ui->setupUi(widget());

    connect(ui->comboBox_SelectPromptProvider, &QComboBox::currentTextChanged, [=](QString text){
        Q_UNUSED(text)
        if (ui->comboBox_SelectPromptProvider->currentIndex() == 1)
        {
            ui->stackedWidget->setCurrentIndex(0);
        }
        else if(ui->comboBox_SelectPromptProvider->currentIndex() == 0)
        {
            ui->stackedWidget->setCurrentIndex(1);
        }
    });

    DEBUG_EXIT(PromptTab::PromptTab);
}

PromptTab::~PromptTab()
{
    DEBUG_ENTER(PromptTab::~PromptTab);
    delete widget();
    DEBUG_EXIT(PromptTab::~PromptTab);
}

void PromptTab::setup(const BashrcSource data)
{
    DEBUG_ENTER(PromptTab::setup);
    QString program = data.program;
    /* Maybe check in bashrc in case they don't want to reconfigure their fancy prompt */
    Searcher searcher(&program, Searcher::StateCheckDoubleQuotations |
                      Searcher::StateCheckSingleQuotations |
                      Searcher::StateCheckSpecialQuotations);
    int promptKeywordStart = searcher.search("prompt-");
    if(!CHECK_SEARCH(promptKeywordStart))
    {
        ui->comboBox_SelectPromptProvider->setCurrentText("Default");
        ui->stackedWidget->setCurrentIndex(1);
        DEBUG << "Selected Default Prompt Provider";
        QRegularExpression regexFindPS1("PS1=('|\")(.{0,})(\\1)");
        // need to do the same for the program's bashrc
        QRegularExpressionMatchIterator iter = regexFindPS1.globalMatch(data.program);
        QRegularExpressionMatch lastMatch;
        bool foundPS1 = iter.hasNext();
        while(iter.hasNext())
        {
            lastMatch = iter.next();
        }
        QString programQuotedText = lastMatch.captured(2);
        DEBUG_VAR(programQuotedText);
        // removing stderr because bash says an error through stderr: bash: cannot set terminal process group (2761): Inappropriate ioctl for device\nbash: no job control in this shell
        ExecuteResult result = runCmd("echo -n \"$PS1\"", true, true); // -n for removing the newline and \"'s for keeping leading whitespace
        DEBUG_VAR(result.output);

        if(!result.output.isEmpty())
        {
            QString PS1 = result.output;
            DEBUG_VAR(PS1);
            if(programQuotedText == PS1.remove("\\n") && foundPS1)
            {
                ui->checkBox_RemoveAllNewlines->setChecked(true);
            }
            else
            {
                ui->checkBox_RemoveAllNewlines->setChecked(false);
            }
        }
        else DEBUG << "PS1 variable not found in QProcess bash -i";
    }
    else
    {
        DEBUG << "Selected Fancy Prompt Prompt Provider";
        ui->comboBox_SelectPromptProvider->setCurrentText("Fancy Prompt");
        program.append("source /usr/local/bin/fancy-prompt.bash\n");
        ui->stackedWidget->setCurrentIndex(0);
        QString promptsize = "prompt-";
        int promptTypeStart = promptKeywordStart + promptsize.size();

        int promptTypeEnd = searcher.search(' ', promptTypeStart);
        if(!CHECK_SEARCH(promptTypeEnd))
        {
            DEBUG_EXIT(PromptTab::setup);
            return;
        }

        QString promptType = searcher.source().mid(promptTypeStart, promptTypeEnd-promptTypeStart);
        promptType[0] = promptType.at(0).toUpper();
        ui->comboBox_SelectFancyPrompt->setCurrentText(promptType);

        QMap<QString, QCheckBox*> flags;
        flags["--ascii"] = ui->checkBox_DisableUnicode;
        flags["--mute"] = ui->checkBox_MutedColors;
        flags["--bold"] = ui->checkBox_BoldLines;
        flags["--compact"] = ui->checkBox_CompactLargePrompts;
        flags["--compact2"] = ui->checkBox_CompactLargePrompts2;
        flags["--double"] = ui->checkBox_DoubleLines;
        flags["--nocolor"] = ui->checkBox_NoColor;
        flags["--parens"] = ui->checkBox_ParensInstead;

        for(QString flag : flags.keys())
        {
            int search = searcher.search(flag);
            flags[flag]->setChecked(CHECK_SEARCH(search));
        }

        QMap<QString, QLineEdit*> textOpts;
        textOpts["--date=\""] = ui->lineEdit_DateFormatText;
        textOpts["--time=\""] = ui->lineEdit_TimeFormatText;
        textOpts["--prompt=\""] = ui->lineEdit_PromptText;
        textOpts["--title=\""] = ui->lineEdit_TitleText;

        for(QString flag : textOpts.keys())
        {
            int search = searcher.search(flag);
            search += flag.length();
            int search2 = searcher.search('\"', search);
            textOpts[flag]->setText(searcher.source().mid(search, search2 - search));
        }

        QMap<QString, QSpinBox*> numOpts;
        numOpts["--right=\""] = ui->spinBox_RightMargin;
        numOpts["--lines=\""] = ui->spinBox_ExtraNewlinesBeforePrompt;

        for(QString flag : numOpts.keys())
        {
            int search = searcher.search(flag);
            search += flag.length();
            int search2 = searcher.search('\"', search);
            numOpts[flag]->setMinimum(0);
            numOpts[flag]->setValue(searcher.source().mid(search, search2 - search).toInt());
        }
    }
    DEBUG_EXIT(PromptTab::setup);
}

BashrcSource PromptTab::exec(const BashrcSource data)
{
    DEBUG_ENTER(PromptTab::exec);
    BashrcSource rtn;
    // Copy contructor seems to work may be issue in future
    rtn = data;

    QString promptCommand;

    if(ui->comboBox_SelectPromptProvider->currentText() == "Fancy Prompt")
    {
        promptCommand.append("source /usr/local/bin/fancy-prompts.bash");
        promptCommand.append("\nprompt-");
        promptCommand.append(ui->comboBox_SelectFancyPrompt->currentText().toLower());
        promptCommand.append(' ');
        promptCommand.append((ui->checkBox_BoldLines->isChecked()) ? "--bold " : "");
        promptCommand.append((ui->checkBox_CompactLargePrompts->isChecked()) ? "--compact " : "");
        promptCommand.append((ui->checkBox_CompactLargePrompts2->isChecked()) ? "--compact2 " : "");
        promptCommand.append((ui->checkBox_DisableUnicode->isChecked()) ? "--ascii " : "");
        promptCommand.append((ui->checkBox_DoubleLines->isChecked()) ? "--double " : "");
        promptCommand.append((ui->checkBox_MutedColors->isChecked()) ? "--mute " : "");
        promptCommand.append((ui->checkBox_NoColor->isChecked()) ? "--nocolor " : "");
        promptCommand.append((ui->checkBox_ParensInstead->isChecked()) ? "--parens " : "");
        promptCommand.append(QString(" --lines=\"%1\"").arg(ui->spinBox_ExtraNewlinesBeforePrompt->value()));
        promptCommand.append(QString(" --right=\"%1\"").arg(ui->spinBox_RightMargin->value()));
        promptCommand.append(QString(" --date=\"%1\"").arg(ui->lineEdit_DateFormatText->text()));
        promptCommand.append(QString(" --time=\"%1\"").arg(ui->lineEdit_TimeFormatText->text()));
        promptCommand.append(QString(" --prompt=\"%1\"").arg(ui->lineEdit_PromptText->text()));
        promptCommand.append(QString(" --title=\"%1\"").arg(ui->lineEdit_TitleText->text()));
        rtn.program.append("\n");
        rtn.program.append(promptCommand);
    }
    else
    {
        if(ui->checkBox_RemoveAllNewlines->isChecked())
        {
            ExecuteResult result = runCmd("echo -n \"$PS1\"", true, true); // -n for removing the newline and \"'s for keeping leading whitespace
            if(!result.output.isEmpty())
            {
                QString PS1 = result.output;
                DEBUG_VAR(PS1);
                rtn.program.append(QString("PS1=\"%1\"").arg(PS1.remove("\\n")));
            }
            else
            {
                int ui = QMessageBox::warning(widget(), NAME + QString(" - Warning"), QString("No Prompt Configurations Found! Can Not Remove Newlines"), QMessageBox::Ok, QMessageBox::NoButton);
                goto end;
            }
        }
    }
    end:
    DEBUG_EXIT(PromptTab::exec);
    return rtn;
}
