#include "LayoutDesignDlg.h"
#include "Plugin.hpp"
#include "CustomNotifier.h"
#include "UIConfigManager.h"
#include <vector>
#include "FileUtils.h"
#include "PluginGlobals.h"
#include "DXFBitmapRenderer.h"
#include "AIDxfRenderer.h"
#include "ListViewColumnFactory.h"
#include "StringUtils.h"
#include "UIUtils.h"


namespace ap {
	LayoutDesignDlg::LayoutDesignDlg() :BaseDialog(),
		m_settings(SettingsContext::Instance().GetLayerDesignSettings()),	
		simpleLV(std::make_unique<SimpleListView<PatternPieceUnit>>()),
		simpleParam(simpleLV->setup.simpleParam),
		pieceLV(std::make_unique<PieceListView<PatternPieceUnit>>()),
		pieceParam(pieceLV->setup.param)
	{
		
		simplePresenter = std::make_unique<SimplePresenter<PatternPieceUnit>>(*simpleLV);		
		piecePresenter = std::make_unique<PiecePresenter>(*pieceLV, pieceParam,*this);
		setup.dialogId = DIALOG_DXF;
	}

	BOOL LayoutDesignDlg::OnInitDialog()
	{
		ASErr error = kNoErr;
		char notifierName[kMaxStringLength];
		error = RegisterNotifier(kFileSelectedNotifier, fFileSelectedNotifier);
		CHECK_ERROR(error);
		error = RegisterNotifier(kOpenFileSelectorNotifier, fOpenFileSelectorNotifier);
		CHECK_ERROR(error);
		error = RegisterNotifier(kCreateDocmentNotifier, fCreateDocmentNotifier);
		CHECK_ERROR(error);
		error = RegisterNotifier(kRenderSettingsNotifier, fRenderSettingsNotifier);
		CHECK_ERROR(error);
		error = RegisterNotifier(kRenderToAIDocmentNotifier, fRenderToAIDocmentNotifier);
		CHECK_ERROR(error);
		error = RegisterNotifier(KDocumentViewZoomNotifier, fDocumentViewZoomNotifier);
		CHECK_ERROR(error);
		
		std::string text = PluginContext::Instance().GetOpenFileContext().GetSelectedFile();
		AppendTitle(hwnd(), L"-" + ToWString(text, Encoding::ANSI));
		// 初始化控件句柄	
		InitControl();
		m_gbShape = std::make_unique<UIGroupBox>();
		m_gbPieces = std::make_unique<UIGroupBox>();
		m_gbJiankou = std::make_unique<UIGroupBox>();
		m_gbObject = std::make_unique<UIGroupBox>();
		m_gbLayout = std::make_unique<UIGroupBox>();
		m_gbShape->Attach(hwnd(), GetDlgItem(hwnd(), gbShape));
		m_gbPieces->Attach(hwnd(), GetDlgItem(hwnd(), gbPieces));
		m_gbJiankou->Attach(hwnd(), GetDlgItem(hwnd(), gbJiankou));
		m_gbObject->Attach(hwnd(), GetDlgItem(hwnd(), gbObject));
		m_gbLayout->Attach(hwnd(), GetDlgItem(hwnd(), gbLayout));
		m_ui.Add(std::move(m_gbShape));
		m_ui.Add(std::move(m_gbPieces));
		m_ui.Add(std::move(m_gbJiankou));
		m_ui.Add(std::move(m_gbObject));
		m_ui.Add(std::move(m_gbLayout));

		//auto m_etFilePath = std::make_unique<UIEdit>();
		//m_etFilePath->Attach(GetDlgItem(hWnd, IDC_FILEPATH_EDIT));
		//m_ui.Add(std::move(m_etFilePath));

		
		InitListView();
		ApplySettingsToUI();
		CenterToAI(hparent(), hwnd());
		OnImportData();
		return TRUE;
	}

	void LayoutDesignDlg::InitControl()
	{
		//combox
		comboxContent = GetDlgItem(hwnd(), cbContent);
		comboxFont = GetDlgItem(hwnd(), cbFont);
		comboxDistance = GetDlgItem(hwnd(), cbDistance);
		comboxReferenceSize = GetDlgItem(hwnd(), cbReferenceSize);
		//btJkColor
		auto m_btColor = std::make_unique<UIButton>();
		
		m_btColor->setup.style.normalStyle.bgColor = AIColorToRGB(m_settings.jiankou.strokeColor);
		m_btColor->Attach(hwnd(), GetDlgItem(hwnd(), btJkColor));
		m_btColor->SetTipText(L"设置剪口的颜色");
	    m_ui.Add(std::move(m_btColor));
		//btOutlineColor
		m_btColor = std::make_unique<UIButton>();
		m_btColor->setup.style.normalStyle.bgColor = AIColorToRGB(m_settings.layout.outlineColor);
		m_btColor->Attach(hwnd(), GetDlgItem(hwnd(), btOutlineColor));
		m_btColor->SetTipText(L"设置轮廓的颜色");
		m_ui.Add(std::move(m_btColor));
		//btcolor1
		m_btColor = std::make_unique<UIButton>();
		m_btColor->setup.style.normalStyle.bgColor = AIColorToRGB(m_settings.object.color1);
		m_btColor->Attach(hwnd(), GetDlgItem(hwnd(), btColor1));
		m_btColor->SetTipText(L"设置字体的颜色");
		m_ui.Add(std::move(m_btColor));
		//btcolor2
		m_btColor = std::make_unique<UIButton>();
		m_btColor->setup.style.normalStyle.bgColor = AIColorToRGB(m_settings.object.color2);
		m_btColor->Attach(hwnd(), GetDlgItem(hwnd(), btColor2));
		m_btColor->SetTipText(L"设置描边的颜色");
		m_ui.Add(std::move(m_btColor));
		//Slider
		SliderParam sph = { 2000,2500,750 };
		SliderParam spv = { 2500,3000,750 };
		SetSliderInt(hwnd(), sliderHSpacing, sph);	
		SetSliderInt(hwnd(), sliderVSpacing, spv);
		SliderParam sp1 = { 0,10000, 1000 };
		SetSliderInt(hwnd(), sliderOutlinePlusWidth, sp1);
		sp1 = { 10,20, 1 };
		SetSliderInt(hwnd(), sliderJkWidth, sp1);
		SetSliderInt(hwnd(), sliderJkLength, sp1);
		sp1 = { 1,5, 1 };
		SetSliderInt(hwnd(), sliderJkLinearity, sp1);
		//SetBitmap
		SetControlBitmap(stJkT, JkTICON);
		SetControlBitmap(stJkV, JkVICON);
		SetControlBitmap(stJkR, JkRICON);
		SetControlBitmap(stJkL, JkLICON);
	}

	void LayoutDesignDlg::InitListView()
	{
		InitListViewConfig(simpleParam,"LayoutDesignDlgParam", "ListView");
		InitListViewConfig(pieceParam,"LayoutDesignDlgParam", "pieceListView");
	}

	void LayoutDesignDlg::InitListViewConfig(LayoutDesignDlgParam &param,const std::string& context, const std::string& sectionName)
	{
		UIConfigManager& manager = UIConfigManager::GetIns();

		std::wstring configPath = FileUtils::GetPluginDir().append(L"\\config\\ui_config.json");
		manager.Initialize(ToString(configPath));

		auto sec = manager.GetSection(context, sectionName);

		param.isAllowConfigUpdate = sec->GetValue<bool>("isAllowConfigUpdate", false);
		param.sizeWidth = sec->GetValue<int>("sizeWidth", 80);
		param.sizeText = sec->GetValue<std::string>("sizeText", "");

		param.pieceText = sec->GetValue<std::string>("pieceText", "");
		param.rowHeight = sec->GetValue<int>("rowHeight", 50);

		param.bitmapPoint = sec->GetValue<AIPoint>("pieceBitmap", { 48,48 });

		param.textColor = sec->GetValue<COLORREF>("textColor", RGB(220, 220, 220));
		param.evenRowColor = sec->GetValue<COLORREF>("evenRowColor", RGB(45, 45, 48));
		param.oddRowColor = sec->GetValue<COLORREF>("oddRowColor", RGB(50, 50, 53));
		param.selectedColor = sec->GetValue<COLORREF>("selectedColor", RGB(75, 110, 175));
		param.selectedNoFocusColor = sec->GetValue<COLORREF>("selectedNoFocusColor", RGB(100, 100, 100));
		param.cellPaddingX = sec->GetValue<int>("cellPaddingX", 6);
		param.pieceInfoTextWidth = sec->GetValue<int>("pieceInfoTextWidth", 120);
		param.pieceSpacing = sec->GetValue<int>("pieceSpacing", 8);
		param.enableRightMenu = sec->GetValue<bool>("enableRightMenu", true);
		param.enablePieceEdit = sec->GetValue<bool>("enablePieceEdit", true);
	}
	
	void LayoutDesignDlg::UpdateComboBoxOptions() {
		if (comboxContent) SendMessage(comboxContent, CB_RESETCONTENT, 0, 0);
		if (comboxFont) SendMessage(comboxFont, CB_RESETCONTENT, 0, 0);
		if (comboxDistance) SendMessage(comboxDistance, CB_RESETCONTENT, 0, 0);
		if (comboxReferenceSize) SendMessage(comboxReferenceSize, CB_RESETCONTENT, 0, 0);
		// 填充内容选项
		const wchar_t* contentOptions[] = { L"${NAME}-${SIZE}", L"${NAME}", L"${SIZE}" };
		for (size_t i = 0; i < sizeof(contentOptions) / sizeof(contentOptions[0]); i++) 
		{
			if (i == 0) SetComboBoxSelection(comboxContent, contentOptions[i]);
			AddComboBoxString(comboxContent, contentOptions[i]);
		}

		// 填充字体选项
		const wchar_t* fontOptions[] = { L"宋体", L"" };
		for (size_t i = 0; i < sizeof(fontOptions) / sizeof(fontOptions[0]);i++) 
		{
			if (i==0) SetComboBoxSelection(comboxFont, fontOptions[i]);
			AddComboBoxString(comboxFont, fontOptions[i]);
		}
		
		// 填充跟随位置选项
		
		const wchar_t* distances[] = { L"上", L"下", L"左", L"右", L"左上", L"右上", L"左下", L"右下" };
		for (size_t i = 0; i < sizeof(distances) / sizeof(distances[0]);i++) {
			if (i == 0) SetComboBoxSelection(comboxDistance, distances[i]);
			AddComboBoxString(comboxDistance, distances[i]);
		}
		// 填充参考尺寸选项
		for (size_t i = 0; i < m_patternPieceUnits.size(); i++)
		{
			std::wstring sizeStr = ToWString(m_patternPieceUnits[i].data.pieces[0].size);
			if(i==0) SetComboBoxSelection(comboxReferenceSize, sizeStr);
			AddComboBoxString(comboxReferenceSize, sizeStr);
		}
		
	}
#if 0
	void LayoutDesignDlg::UpdateEditDlgComboBoxOptions()
	{
		auto& pieceEditDlg = simpleLV->GetPieceEditDlg();
		if (pieceEditDlg->hwnd())
		{
			for (size_t i = 0; i < m_patternPieceUnits.size(); i++) {
				pieceEditDlg->SetComboBoxOptions([&]()
					{
						PieceEditDlg::PieceEditCbInfo info;
						info.type = ComboBoxType::Size;
						info.value = ToWString(m_patternPieceUnits[i].data.pieces[0].size);
						return info;
					}
				);
				pieceEditDlg->SetComboBoxOptions([&]()
					{
						PieceEditDlg::PieceEditCbInfo info;
						info.type = ComboBoxType::Fabric;
						info.value = ToWString(m_patternPieceUnits[i].data.pieces[0].material);
						return info;
					}
				);
				pieceEditDlg->SetComboBoxOptions([&]()
					{
						PieceEditDlg::PieceEditCbInfo info;
						info.type = ComboBoxType::Category;
						info.value = ToWString(m_patternPieceUnits[i].data.pieces[0].category);
						return info;
					}
				);
			}
		}
	}
#endif
	void LayoutDesignDlg::InitSimpleColumn()
	{
		simpleLV->ClearColumn();
		simpleLV->Attach(hwnd(), GetDlgItem(hwnd(), IDC_LAYOUT_DESIGN_LIST), hinstance(), simpleParam.rowHeight);
		if (listViewColumnInitialized && !simpleParam.isAllowConfigUpdate) return;
		ListViewColumn<PatternPieceUnit> sizeCol(
			ToWString(simpleParam.sizeText,Encoding::UTF8),
			AIPoint{ simpleParam.sizeWidth, simpleParam.rowHeight },
			
			LVCFMT_LEFT,
			[](const PatternPieceUnit& unit) -> std::wstring {
				return std::wstring(unit.size.begin(), unit.size.end());
			}
		);
		sizeCol.SetHasCheckbox(); // set this column show checkbox
		simplePresenter->AddColumn(sizeCol);

		ListViewColumn<PatternPieceUnit> pieceCol(
			ToWString(simpleParam.pieceText, Encoding::UTF8),
			AIPoint{ simpleParam.pieceWidth,simpleParam.rowHeight },
			LVCFMT_CENTER,
			[](const PatternPieceUnit& row) -> std::vector<HBITMAP> {
				std::vector<HBITMAP> result;
				for (auto& piece : row.pieces) {
					if (piece.simpleImage) result.push_back(piece.simpleImage);
				}
				return result;
			}
		);
		//set sort
		pieceCol.comparator = [](const PatternPieceUnit& a, const PatternPieceUnit& b) {
			if (a.pieces.empty() || b.pieces.empty()) return false;
			return a.pieces[0].name < b.pieces[0].name;
			};
		// set info extractor
		pieceCol.SetInfoExtractor([](const PatternPieceUnit& unit, int colIndex) -> std::vector<std::wstring> {
			std::vector<std::wstring> infos;
			for (const auto& piece : unit.pieces) {
				std::wstring name = ToWString(piece.name);
				std::wstring material = ToWString(piece.material);
				std::wstring category = ToWString(piece.category);
				// 每个裁片添加3条信息：名称、材料、类别
				infos.push_back(name);
				infos.push_back(material);
				infos.push_back(category);
			}
			return infos;
			});
		pieceCol.SetImageSize(simpleParam.bitmapPoint);



		simplePresenter->AddColumn(pieceCol);

		listViewColumnInitialized = true;
	}
#if 0
	void LayoutDesignDlg::InitPieceColumn()
	{
		pieceLV->ClearColumn();
		pieceLV->Attach(hwnd(), GetDlgItem(hwnd(), IDC_FLPPIECE_LIST), hinstance(), pieceParam.rowHeight);
		if (!pieceParam.isAllowConfigUpdate) return;
		colums.clear();
		auto& pieces = m_patternPieceUnits[0].data.pieces;
		for (size_t i=0;i< pieces.size();i++)
		{
			auto& colum = pieceLV->MakeImageColumn(
				ToWString(pieces[i].defineName),
				pieceParam.pieceBitmap.size.cx,
				pieceParam.pieceBitmap.size.cy,
				[&](const PatternPieceUnit& unit)->std::vector<HBITMAP> {
					std::vector<HBITMAP> result;
					for (auto& piece : unit.pieces)
					{
						result.push_back(piece.thumb);
					}
					return result;
				},
				[&](const PatternPieceUnit& unit, size_t )->std::vector<std::wstring> {
					std::vector<std::wstring> result;
					for (auto& piece : unit.pieces)
					{
						result.push_back(ToWString(piece.defineName));
					}
					return result;
				}
			);
			colums.push_back(colum);			
		}
		piecePresenter->AddColumn(colums);
	}
#endif
	void LayoutDesignDlg::AddSimpleItem(const std::unordered_map<std::string, std::vector<PatternPiece>>& shapeMap)
	{
		if (shapeMap.empty())return;
		//Calculate the column width of the ImageView after importing the data.
		simpleParam.pieceWidth = shapeMap.begin()->second.size() * (simpleParam.bitmapPoint.h + simpleParam.pieceInfoTextWidth + simpleParam.pieceSpacing) + simpleParam.pieceSpacing;
		InitSimpleColumn();
		// Add to list view
		m_patternPieceUnits.clear();
		std::vector<PieceEditDlg::PieceEditCbInfo> infos;
		bool definePiece = true;
		for (const auto& shape : shapeMap)
		{

			if (shape.first.empty()) continue;
			PatternPieceUnit patternPieceUnit;
			patternPieceUnit.size = shape.first;
			patternPieceUnit.sizeChecked = false;
			patternPieceUnit.pieces = shape.second;
			m_patternPieceUnits.push_back({ patternPieceUnit });
			// Prepare combo box options
			PieceEditDlg::PieceEditCbInfo info;
			info.type = ComboBoxType::Size;
			info.value = ToWString(patternPieceUnit.pieces[0].size);
			infos.push_back(info);
			if (definePiece)
			{
				for(auto& sizePiece: patternPieceUnit.pieces)
				{
					PieceEditDlg::PieceEditCbInfo info;
					info.type = ComboBoxType::Fabric;
					info.value = ToWString(sizePiece.material);
					infos.push_back(info);
					info.type = ComboBoxType::Category;
					info.value = ToWString(sizePiece.category);
					infos.push_back(info);
				}
				definePiece = false;
			}
		}
		simpleLV->GetPieceEditDlg()->RegisterComboBoxOptionsSet([infos]() { return infos; });
		piecePresenter->SortItem(m_patternPieceUnits, [](PatternPieceUnit& unit)
			{
				    std::stable_sort(unit.pieces.begin(), unit.pieces.end(),
					[](const PatternPiece& a, const PatternPiece& b)
					{
						return a.name < b.name;
							
					});
			}
		);
		simplePresenter->SetItems(m_patternPieceUnits);
	}

	void LayoutDesignDlg::AddPieceItem()
	{

		pieceParam.columWidth = pieceParam.bitmapPoint.h + pieceParam.pieceSpacing;
		piecePresenter->SetItems(m_patternPieceUnits);
	}

	JKMode LayoutDesignDlg::GetJiankouMode()
	{
		if (IsDlgButtonChecked(hwnd(), rbJkT) == BST_CHECKED) return JKMode::T;
		if (IsDlgButtonChecked(hwnd(), rbJkV) == BST_CHECKED) return JKMode::V;
		if (IsDlgButtonChecked(hwnd(), rbJkR) == BST_CHECKED) return JKMode::R;
		if (IsDlgButtonChecked(hwnd(), rbJkL) == BST_CHECKED) return JKMode::L;

		return JKMode::T;
	}

	void LayoutDesignDlg::ReadSettingsFromUI()
	{
		// Shape GroupBox
		m_settings.shape.materiSelected = IsDlgButtonChecked(hwnd(), btMateriSlt) == BST_CHECKED;
		m_settings.shape.shapeChecked = IsDlgButtonChecked(hwnd(), btShapeCheck) == BST_CHECKED;
		m_settings.shape.removeNameChar = IsDlgButtonChecked(hwnd(), btRmNameChar) == BST_CHECKED;
		m_settings.shape.selectRmNameChar = IsDlgButtonChecked(hwnd(), btSltRmNameChar) == BST_CHECKED;
		m_settings.shape.sortAsc = m_sortAsc;

		// Jiankou GroupBox
		m_settings.jiankou.enabled = IsDlgButtonChecked(hwnd(), cbJiankou) == BST_CHECKED;
		m_settings.jiankou.mode = GetJiankouMode();
		m_settings.jiankou.width = (int)SendDlgItemMessage(hwnd(), sliderJkWidth, TBM_GETPOS, 0, 0);
		m_settings.jiankou.length = (int)SendDlgItemMessage(hwnd(), sliderJkLength, TBM_GETPOS, 0, 0);
		m_settings.jiankou.linearity = (int)SendDlgItemMessage(hwnd(), sliderJkLinearity, TBM_GETPOS, 0, 0);

		// Object GroupBox
		m_settings.object.ttEnable = IsDlgButtonChecked(hwnd(), cbTtEnable) == BST_CHECKED;
		int contentIndex = (int)SendDlgItemMessage(hwnd(), cbContent, CB_GETCURSEL, 0, 0);
		int fontIndex = (int)SendDlgItemMessage(hwnd(), cbFont, CB_GETCURSEL, 0, 0);

		// 根据索引获取文本
		if (contentIndex != CB_ERR) {
			wchar_t buffer[256] = { 0 };
			SendDlgItemMessageW(hwnd(), cbContent, CB_GETLBTEXT, contentIndex, (LPARAM)buffer);
			m_settings.object.content = buffer;
		}

		if (fontIndex != CB_ERR) {
			wchar_t buffer[256] = { 0 };
			SendDlgItemMessageW(hwnd(), cbFont, CB_GETLBTEXT, fontIndex, (LPARAM)buffer);
			m_settings.object.font = buffer;
		}
		m_settings.object.size = (int)SendDlgItemMessage(hwnd(), sliderSize, TBM_GETPOS, 0, 0);
		// color1, color2, strokeWidth, distance 等可类似获取

		// Layout GroupBox
		m_settings.layout.hSpacing = (int)SendDlgItemMessage(hwnd(), sliderHSpacing, TBM_GETPOS, 0, 0);
		m_settings.layout.vSpacing = (int)SendDlgItemMessage(hwnd(), sliderVSpacing, TBM_GETPOS, 0, 0);
		int pos = (int)SendDlgItemMessage(hwnd(), sliderOutlinePlusWidth, TBM_GETPOS, 0, 0);
		m_settings.layout.outlinePlusWidth = pos / 1000.00;
		m_settings.layout.autoClearInternalLines = IsDlgButtonChecked(hwnd(), cbAutoClearInternalLines) == BST_CHECKED;
		m_settings.layout.preserveOriginalGraphicToTopHiddenLayer = IsDlgButtonChecked(hwnd(), cbPreserveOriginalGraphicToTopHiddenLayer) == BST_CHECKED;
		m_settings.layout.referenceSizeChecked = IsDlgButtonChecked(hwnd(), cbReferenceSize) == BST_CHECKED;
		m_settings.layout.referenceSize = (int)SendDlgItemMessage(hwnd(), cbReferenceSize, TBM_GETPOS, 0, 0);
		// outlineColor 可通过 ColorDialog 或自定义方式读取
	}

	void LayoutDesignDlg::ApplySettingsToUI()
	{
		// Shape
		CheckDlgButton(hwnd(), btMateriSlt, m_settings.shape.materiSelected ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwnd(), btShapeCheck, m_settings.shape.shapeChecked ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwnd(), btRmNameChar, m_settings.shape.removeNameChar ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwnd(), btSltRmNameChar, m_settings.shape.selectRmNameChar ? BST_CHECKED : BST_UNCHECKED);

		// Jiankou
		CheckDlgButton(hwnd(), cbJiankou, m_settings.jiankou.enabled ? BST_CHECKED : BST_UNCHECKED);
		CheckRadioButton(hwnd(), rbJkT, rbJkL, rbJkT);
		SendDlgItemMessage(hwnd(), sliderJkWidth, TBM_SETPOS, TRUE, m_settings.jiankou.width);
		SendDlgItemMessage(hwnd(), sliderJkLength, TBM_SETPOS, TRUE, m_settings.jiankou.length);
		SendDlgItemMessage(hwnd(), sliderJkLinearity, TBM_SETPOS, TRUE, m_settings.jiankou.linearity);

		// Object
		CheckDlgButton(hwnd(), cbTtEnable, m_settings.object.ttEnable ? BST_CHECKED : BST_UNCHECKED);
		if (!m_settings.object.content.empty()) {
			SetDlgItemText(hwnd(), cbContent, m_settings.object.content.c_str());
			// 查找并选中匹配的项
			LRESULT result = SendDlgItemMessage(hwnd(), cbContent, CB_SELECTSTRING,
				(WPARAM)-1, (LPARAM)m_settings.object.content.c_str());
			if (result == CB_ERR) {
				// 如果没找到匹配项，设置为编辑框文本（不选中列表项）
				SetDlgItemText(hwnd(), cbContent, m_settings.object.content.c_str());
			}
		}

		if (!m_settings.object.font.empty()) {
			SetDlgItemText(hwnd(), cbFont, m_settings.object.font.c_str());
			LRESULT result = SendDlgItemMessage(hwnd(), cbFont, CB_SELECTSTRING,
				(WPARAM)-1, (LPARAM)m_settings.object.font.c_str());
			if (result == CB_ERR) {
				SetDlgItemText(hwnd(), cbFont, m_settings.object.font.c_str());
			}
		}
		SendDlgItemMessage(hwnd(), sliderSize, TBM_SETPOS, TRUE, m_settings.object.size);

		// Layout
		SendDlgItemMessage(hwnd(), sliderHSpacing, TBM_SETPOS, TRUE, m_settings.layout.hSpacing);
		SendDlgItemMessage(hwnd(), sliderVSpacing, TBM_SETPOS, TRUE, m_settings.layout.vSpacing);
		SendDlgItemMessage(hwnd(), sliderOutlinePlusWidth, TBM_SETPOS, TRUE, m_settings.layout.outlinePlusWidth);
		CheckDlgButton(hwnd(), cbAutoClearInternalLines, m_settings.layout.autoClearInternalLines ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwnd(), cbPreserveOriginalGraphicToTopHiddenLayer, m_settings.layout.preserveOriginalGraphicToTopHiddenLayer ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwnd(), cbReferenceSize, m_settings.layout.referenceSizeChecked ? BST_CHECKED : BST_UNCHECKED);
		SendDlgItemMessage(hwnd(), cbReferenceSize, TBM_SETPOS, TRUE, m_settings.layout.referenceSize);
	}

	void LayoutDesignDlg::SetControlColor(int id, AIColor& color)
	{
		//AIColor color = {};
		ASErr err = sAIPaintStyle->SetCurrentColor(&color);
		if (err) return;
		if (sAIPaintStyle->DisplayColorPicker(&color))
		{
			COLORREF rgb = AIColorToRGB(color);
			// 用户选择了颜色，保存到设置
			UIButton* btn = dynamic_cast<UIButton*>(m_ui.FromId(id));
			if (btn)
			{
				btn->setup.style.normalStyle.bgColor = rgb;
				btn->UpdateStyle();
			}
		}
	}
#if 0
	void LayoutDesignDlg::SetBtColor(int id, COLORREF& color)
	{
		CHOOSECOLOR cc = { 0 };
		static COLORREF customColors[16] = { 0 };

		cc.lStructSize = sizeof(cc);
		cc.hwndOwner = hwnd(); // 对话框句柄
		cc.lpCustColors = customColors;
		cc.rgbResult = color;
		cc.Flags = CC_FULLOPEN | CC_RGBINIT;

		if (ChooseColor(&cc))
		{
			// 用户选择了颜色，保存到设置
			color = cc.rgbResult;
			UIButton* btn = dynamic_cast<UIButton*>(m_ui.FromId(id));
			if (btn)
			{
				btn->setup.style.normalStyle.bgColor = cc.rgbResult;
				btn->UpdateStyle();
			}


		}
	}
#endif
#if 0
	void LayoutDesignDlg::InitListViewEvents()
	{
		// 设置右键菜单
		ListViewEvents<PatternPieceUnit> ev;
#if 0
		// 右键点击事件（整行）
		ev.onRightClick = [this](HWND hwnd, int row, POINT pt) {
			HMENU menu = CreatePopupMenu();
			AppendMenuW(menu, MF_STRING, 1001, L"保存图块");
			AppendMenuW(menu, MF_STRING, 1002, L"删除图块");

			TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
			DestroyMenu(menu);
			};
#endif

		// 新增：裁片右键点击事件
		ev.onPieceRightClick = [this](HWND hwnd, int row, int pieceIndex, POINT pt) {
			if (row >= 0 && row < static_cast<int>(m_patternPieceUnits.size())) {
				const auto& unit = m_patternPieceUnits[row];
				if (pieceIndex >= 0 && pieceIndex < static_cast<int>(unit.pieces.size())) {
					// 创建裁片编辑菜单
					HMENU menu = CreatePopupMenu();

					wchar_t menuText[256];
					wsprintfW(menuText, L"编辑裁片: %s", ToWString(unit.pieces[pieceIndex].name).c_str());
					AppendMenuW(menu, MF_STRING, 2000 + pieceIndex * 10 + 0, menuText);
					AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
					AppendMenuW(menu, MF_STRING, 2000 + pieceIndex * 10 + 1, L"设置为有效/无效");

					// 显示菜单
					BOOL result = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
						pt.x, pt.y, 0, hwnd, nullptr);

					if (result > 0) {
						// 处理菜单选择
						int menuId = result;
						int baseId = 2000 + pieceIndex * 10;

						if (menuId == baseId) {
							// 编辑整个裁片
							EditPatternPiece(row, pieceIndex);
						}
						else if (menuId == baseId + 1) {
							// 编辑名称
							EditPieceName(row, pieceIndex);
						}
						else if (menuId == baseId + 2) {
							// 编辑材料
							EditPieceMaterial(row, pieceIndex);
						}
						else if (menuId == baseId + 3) {
							// 编辑类别
							EditPieceCategory(row, pieceIndex);
						}
					}

					DestroyMenu(menu);
				}
			}
			};

		ev.onDoubleClick = [](HWND hwnd, int row) {
			MessageBoxW(hwnd, L"图块被双击！", L"信息", MB_OK);
			};

		ev.onImageClick = [](HWND hwnd, int row, int col, int imageIndex) {
			WCHAR msg[256];
			wsprintfW(msg, L"点击了第 %d 行，第 %d 列的第 %d 个图片", row + 1, col + 1, imageIndex + 1);
			MessageBoxW(hwnd, msg, L"图片点击", MB_OK);
			};

		// 复选框事件
		ev.onGetCheckboxState = [this](HWND hwnd, int row) -> bool {
			if (row >= 0 && row < static_cast<int>(m_patternPieceUnits.size())) {
				return m_patternPieceUnits[row].sizeChecked;
			}
			return false;
			};

		ev.onCheckboxChanged = [this](HWND hwnd, int row, bool checked) {
			if (row >= 0 && row < static_cast<int>(m_patternPieceUnits.size())) {
				m_patternPieceUnits[row].sizeChecked = checked;

				// 输出调试信息
				WCHAR debugMsg[256];
				wsprintfW(debugMsg, L"复选框状态改变: 行=%d, 状态=%s\n",
					row + 1, checked ? L"选中" : L"未选中");
				OutputDebugStringW(debugMsg);

				// 重绘整行
				RECT rcRow;
				if (ListView_GetItemRect(hwnd, row, &rcRow, LVIR_BOUNDS)) {
					InvalidateRect(hwnd, &rcRow, TRUE);
					UpdateWindow(hwnd);
				}
			}
			};
		// 裁片选中状态事件
		ev.onGetPieceSelectedState = [this](HWND hwnd, int row, int pieceIndex) -> bool {
			if (row >= 0 && row < static_cast<int>(m_patternPieceUnits.size())) {
				const auto& unit = m_patternPieceUnits[row];
				if (pieceIndex >= 0 && pieceIndex < static_cast<int>(unit.pieces.size())) {
					// 如果尺码被全选，则所有裁片都被选中
					if (unit.sizeChecked) {
						return true;
					}
					// 否则检查裁片是否被单独选中
					// 这里需要你添加裁片选中状态的数据存储
					// 例如：在 PatternPiece 中添加 bool selected 字段
					// 或者使用一个映射来存储选中状态
					return unit.pieces[pieceIndex].selected; // 假设 PatternPiece 添加了 selected 字段
				}
			}
			return false;
			};

		ev.onPieceSelectionChanged = [this](HWND hwnd, int row, int pieceIndex, bool selected) {
			if (row >= 0 && row < static_cast<int>(m_patternPieceUnits.size())) {
				auto& unit = m_patternPieceUnits[row];
				if (pieceIndex >= 0 && pieceIndex < static_cast<int>(unit.pieces.size())) {
					// 更新裁片选中状态
					unit.pieces[pieceIndex].selected = selected;

					// 重绘该行
					RECT rcRow;
					if (ListView_GetItemRect(hwnd, row, &rcRow, LVIR_BOUNDS)) {
						InvalidateRect(hwnd, &rcRow, TRUE);
					}
				}
			}
			};
		ev.onDrawItem = [](HWND hwnd, int row, const RECT& rc, HDC hdc) {
			RECT r = rc;
			r.right = r.left + 100;
			r.bottom = r.top + 100;
			FillRect(hdc, &r, (HBRUSH)(COLOR_GRAYTEXT + 1));
			};
		simpleLV->SetEvents(ev);
	}
#endif
	void LayoutDesignDlg::OnCommand(WORD id, WORD code, HWND hCtrl)
	{
		if (id == btMateriSlt && code == BN_CLICKED)
		{
			simpleLV->UpdateSelectedPieces([](PatternPiece& piece) {
				piece.material = "面料B";
				});
		}
		else if (id == btShapeCheck && code == BN_CLICKED)
		{
			
		}
		else if (id == btColor1 && code == BN_CLICKED)
		{
			SetControlColor(btColor1, m_settings.object.color1);
		}
		else if (id == btColor2 && code == BN_CLICKED)
		{
			SetControlColor(btColor2, m_settings.object.color2);
		}
		else if (id == btOutlineColor && code == BN_CLICKED)
		{
			SetControlColor(btOutlineColor, m_settings.layout.outlineColor);
		}
		else if (id == btSltRmNameChar && code == BN_CLICKED)
		{

		}
		else if (id == btRmNameChar && code == BN_CLICKED)
		{

		}
		else if (id == btSort && code == BN_CLICKED)
		{
			bool asc = m_sortAsc;

			simpleLV->SortEachItem(
				[asc](PatternPieceUnit& unit)
				{
					std::stable_sort(unit.pieces.begin(), unit.pieces.end(),
						[asc](const PatternPiece& a, const PatternPiece& b)
						{
							return asc ? a.name < b.name
								: a.name > b.name;
						});
				});

			m_sortAsc = !m_sortAsc;
		}
		else if (id == btJkColor && code == BN_CLICKED)
		{
			SetControlColor(btJkColor, m_settings.jiankou.strokeColor);
			//SetBtColor(btJkColor, m_settings.jiankou.strokeColor);
		}
		else if (id == btImportGrap && code == BN_CLICKED)
		{
			auto items = piecePresenter->GetItems();
			ReadSettingsFromUI();
			int deltaX = m_settings.layout.hSpacing;
			int deltaY = m_settings.layout.vSpacing;
			AIRealPoint space = { deltaX,deltaY };
			sAINotifier->Notify(
				kRenderSettingsNotifier,
				(void*)&space
			);
			sAINotifier->Notify(
				kRenderToAIDocmentNotifier,
				(void*)&items
			);
			OnClose();
			AIReal zoom = { 0.0625 };
			sAINotifier->Notify(
				KDocumentViewZoomNotifier,
				&zoom
			);
		}
		
		return;
	}

	void LayoutDesignDlg::OnImportData()
	{
		sAINotifier->Notify(
			kCreateDocmentNotifier,
			nullptr
		);
		auto& ctx = PluginContext::Instance().GetDxfDocContext(PluginContext::Instance().GetOpenFileContext().GetSelectedFile());
		AddSimpleItem(LoadBlocks(std::move(ctx)));
		AddPieceItem();
		UpdateComboBoxOptions();
	}

	LRESULT LayoutDesignDlg::OnNotify(LPARAM lParam)
	{
		simpleLV->OnNotify(hwnd(), lParam);
		return pieceLV->OnNotify(hwnd(), lParam);
	}

	void LayoutDesignDlg::OnShowWindow(WPARAM wParam)
	{
		if (wParam && !initialized)
		{
			FixGroupBoxChildren(
				hwnd(),
				gbShape,
				{
					btMateriSlt,
					btShapeCheck,
					btSltRmNameChar,
					btRmNameChar,
					btSort
				}
			);
			FixGroupBoxChildren(
				hwnd(),
				gbPieces,
				{

				}
				);
			FixGroupBoxChildren(
				hwnd(),
				gbJiankou,
				{
				   cbJiankou,
				   rbJkT    ,
				   rbJkV    ,
				   rbJkR    ,
				   rbJkL    ,
				   stJkT    ,
				   stJkV    ,
				   stJkR    ,
				   stJkL    ,
				   uiLabel1 ,
				   uiLabel2 ,
				   uiLabel3 ,
				   uiLabel4 ,
				   sliderJkWidth  ,
				   sliderJkLength  ,
				   sliderJkLinearity,
				   btJkColor
				}
			);
			FixGroupBoxChildren(
				hwnd(),
				gbObject,
				{
					cbTtEnable   ,
					stContent    ,
					cbContent    ,
					stFont       ,
					cbFont       ,
					stSize       ,
					sliderSize   ,
					stColor1     ,
					btColor1     ,
					stStrokeWidth,
					slider5      ,
					stColor2     ,
					btColor2     ,
					stDistance   ,
					cbDistance
				}
			);

			FixGroupBoxChildren(
				hwnd(),
				gbLayout,
				{
					stHSpacing,
					sliderHSpacing,
					stVSpacing,
					sliderVSpacing,
					stOutlinePlusWidth,
					sliderOutlinePlusWidth,
					cbAutoClearInternalLines,
					cbPreserveOriginalGraphicToTopHiddenLayer,
					stReferenceSize,
					cbReferenceSize,
					stOutlineColor,
					btImportGrap
				}
			);

			initialized = true;
		}
	}

	LRESULT  LayoutDesignDlg::OnDrawItem(LPDRAWITEMSTRUCT dis)
	{
		return BaseDialog::OnDrawItem(dis);;
	}

	BOOL LayoutDesignDlg::OnClose(int nResult)
	{

		simplePresenter->Clear();//->ClearColumn();
		piecePresenter->Clear();//->ClearColumn();

		return BaseDialog::OnClose();
	}

	void LayoutDesignDlg::OnSize(UINT state, int cx, int cy)
	{

	}

	const std::unordered_map<std::string, std::vector<PatternPiece>> LayoutDesignDlg::LoadBlocks(const std::vector<DxfBlock>& blocks)
	{
		// Group blocks by size
		std::unordered_map<std::string, std::vector<PatternPiece>> shapeMap;
		int blocksCount = blocks.size();
		for (size_t i = 0; i < blocksCount; i++)
		{
			PatternPiece sp;
			const std::string& s = blocks[i].name;
			size_t pos = s.find_last_of('.');
			std::string tail = (pos == std::string::npos) ? s : s.substr(pos + 1);
			DXFBitmapRenderer renderer;
			HBITMAP master = nullptr;// DXFBitmapRenderer::RenderMaster(ctx, blocks[i], 2048);
			HBITMAP previewImage= renderer.Scale(master, simpleParam.bitmapPoint.h, simpleParam.bitmapPoint.v);
			HBITMAP thumb = renderer.Scale(master, pieceParam.bitmapPoint.h, pieceParam.bitmapPoint.v);


			//HBITMAP bmp = RenderDXFToBitmap_GDIPlus(blocks[i], dlgParam.pieceBitmap.size.cx, dlgParam.pieceBitmap.size.cy);
			//if (!bmp) {
			//	bmp = CreateTestBitmap(48, 48, RGB(200, 200, 200));
			//}
		
			sp.id = static_cast<int>(i);;
			sp.size = tail;
			sp.name = s;
			sp.material = "面料A";  // 默认值
			sp.category = "类别";  // 默认值
			sp.selected = false; // 初始化选中状态为false
			sp.thumb = thumb;
			sp.previewImage = previewImage;
			sp.shapeImage = master;
			shapeMap[sp.size].push_back(sp);
		}
		return shapeMap;
	}

	const std::unordered_map<std::string, std::vector<PatternPiece>> LayoutDesignDlg::LoadBlocks(const DxfDocumentContext& ctx)
	{
		const std::unordered_map<std::string, DxfBlock>& blocks = ctx.GetDxf().blocks;
		// Group blocks by size
		std::unordered_map<std::string, std::vector<PatternPiece>> shapeMap;
		for (const auto& [name, block] : blocks)
		{
			std::string size, defineName;
			PatternPiece sp;
			ParseName(name, defineName, size);
			DXFBitmapRenderer renderer;
			HBITMAP master = renderer.RenderMaster(ctx,block,sp, 256);
			HBITMAP previewImage = renderer.Scale(master, 160, 170);
			HBITMAP simpleImage = renderer.Scale(master, simpleParam.bitmapPoint.h, simpleParam.bitmapPoint.v);
			HBITMAP thumb = renderer.Scale(master, pieceParam.bitmapPoint.h, pieceParam.bitmapPoint.v);
			sp.size = size;
			sp.name = name;
			sp.defineName = defineName;// defineName;  // 默认值
			sp.selected = false; 
			sp.previewImage = previewImage;
			sp.simpleImage = simpleImage;
			sp.thumb = thumb;
			sp.shapeImage = master;
			shapeMap[sp.size].push_back(sp);
		}
		return shapeMap;
	}

	void LayoutDesignDlg::EditPieceName(int row, int pieceIndex)
	{
		// 实现名称编辑
	}

	void LayoutDesignDlg::EditPieceMaterial(int row, int pieceIndex)
	{
		// 实现材料编辑
	}

	void LayoutDesignDlg::EditPieceCategory(int row, int pieceIndex)
	{
		// 实现类别编辑
	}

	bool LayoutDesignDlg::GetCheckboxStateFromData(int row)
	{
		// 这里需要根据你的数据结构获取复选框状态
		// 例如，如果你有一个 vector<PatternPieceUnit> 存储数据
		if (row >= 0 && row < static_cast<int>(m_patternPieceUnits.size())) {
			return m_patternPieceUnits[row].data.sizeChecked;
		}
		return false;
	}

	void LayoutDesignDlg::SetCheckboxStateToData(int row, bool checked)
	{
		// 这里需要根据你的数据结构设置复选框状态
		if (row >= 0 && row < static_cast<int>(m_patternPieceUnits.size())) {
			m_patternPieceUnits[row].data.sizeChecked = checked;
		}
	}
	
	void LayoutDesignDlg::ParseName(const std::string& name, std::string& defineName, std::string& size)
	{
		auto last = name.rfind('.');
		if (last != std::string::npos)
		{
			size =name.substr(last + 1);
			auto second = name.rfind('.', last - 1);
			if (second != std::string::npos)
				defineName = name.substr(second + 1, last - second - 1);
		}
		else
		{
			size = name;
		}
	}


}