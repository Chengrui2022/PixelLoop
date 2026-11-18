import AppKit
import UniformTypeIdentifiers
import WebKit

@main
final class ArnoldLab: NSObject, NSApplicationDelegate, WKUIDelegate, WKNavigationDelegate, WKScriptMessageHandler {
    private var window: NSWindow?
    private var pageURL: URL?

    static func main() {
        let app = NSApplication.shared
        let delegate = ArnoldLab()
        app.delegate = delegate
        app.setActivationPolicy(.regular)
        app.run()
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        installMenu()
        guard let url = Bundle.main.url(forResource: "index", withExtension: "html", subdirectory: "ui") else {
            let alert = NSAlert()
            alert.messageText = "无法打开实验界面"
            alert.informativeText = "应用缺少界面文件。请重新复制完整的 .app。"
            alert.runModal()
            NSApp.terminate(nil)
            return
        }

        pageURL = url
        let configuration = WKWebViewConfiguration()
        configuration.websiteDataStore = .nonPersistent()
        configuration.userContentController.add(self, name: "savePNG")

        let web = WKWebView(frame: .zero, configuration: configuration)
        web.uiDelegate = self
        web.navigationDelegate = self
        web.loadFileURL(url, allowingReadAccessTo: url.deletingLastPathComponent())

        let frame = NSRect(x: 0, y: 0, width: 1090, height: 720)
        let appWindow = NSWindow(contentRect: frame,
                                 styleMask: [.titled, .closable, .miniaturizable, .resizable],
                                 backing: .buffered,
                                 defer: false)
        appWindow.title = "Arnold 图像置乱实验"
        appWindow.contentMinSize = NSSize(width: 540, height: 450)
        appWindow.contentView = web
        appWindow.center()
        appWindow.makeKeyAndOrderFront(nil)
        window = appWindow
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }

    private func installMenu() {
        let menuBar = NSMenu()
        let appItem = NSMenuItem()
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "退出 Arnold 图像置乱实验", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appItem.submenu = appMenu
        menuBar.addItem(appItem)

        let editItem = NSMenuItem()
        let editMenu = NSMenu(title: "编辑")
        editMenu.addItem(withTitle: "剪切", action: #selector(NSText.cut(_:)), keyEquivalent: "x")
        editMenu.addItem(withTitle: "复制", action: #selector(NSText.copy(_:)), keyEquivalent: "c")
        editMenu.addItem(withTitle: "粘贴", action: #selector(NSText.paste(_:)), keyEquivalent: "v")
        editMenu.addItem(withTitle: "全选", action: #selector(NSText.selectAll(_:)), keyEquivalent: "a")
        editItem.submenu = editMenu
        menuBar.addItem(editItem)
        NSApp.mainMenu = menuBar
    }

    func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction,
                 decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
        let allowed = navigationAction.request.url?.standardizedFileURL == pageURL?.standardizedFileURL
        decisionHandler(allowed ? .allow : .cancel)
    }

    func webView(_ webView: WKWebView, runOpenPanelWith parameters: WKOpenPanelParameters,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping ([URL]?) -> Void) {
        guard let window else {
            completionHandler(nil)
            return
        }
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        panel.allowedContentTypes = [.png, .jpeg]
        panel.beginSheetModal(for: window) { response in
            completionHandler(response == .OK ? panel.urls : nil)
        }
    }

    func userContentController(_ controller: WKUserContentController, didReceive message: WKScriptMessage) {
        guard message.name == "savePNG", message.frameInfo.isMainFrame,
              let body = message.body as? [String: String],
              let proposedName = body["name"],
              let encoded = body["base64"], encoded.count < 8_000_000,
              let png = Data(base64Encoded: encoded),
              png.starts(with: [137, 80, 78, 71, 13, 10, 26, 10]),
              let window else { return }

        let panel = NSSavePanel()
        panel.allowedContentTypes = [.png]
        panel.nameFieldStringValue = URL(fileURLWithPath: proposedName).lastPathComponent
        panel.beginSheetModal(for: window) { [weak self, weak web = message.webView] response in
            guard response == .OK, let url = panel.url else { return }
            do {
                try png.write(to: url, options: .atomic)
                self?.report("已保存 " + url.lastPathComponent, ok: true, to: web)
            } catch {
                self?.report("无法保存 PNG。请检查目标文件夹是否可写。", ok: false, to: web)
            }
        }
    }

    private func report(_ text: String, ok: Bool, to web: WKWebView?) {
        guard let data = try? JSONSerialization.data(withJSONObject: [text, ok]),
              let arguments = String(data: data, encoding: .utf8) else { return }
        web?.evaluateJavaScript("window.arnoldSaveStatus(...\(arguments));")
    }
}
