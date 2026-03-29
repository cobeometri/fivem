import "./bootstrap";
import { HashRouter, Route, Routes } from "react-router-dom";

import { registerMpMenuServersService } from "cfx/apps/mpMenu/services/servers/servers.mpMenu";
import { startBrowserApp } from "cfx/base/createApp";
import { IIntlService } from "cfx/common/services/intl/intl.service";
import { registerLogService } from "cfx/common/services/log/logService";
import { ConsoleLogProvider } from "cfx/common/services/log/providers/consoleLogProvider";
import { ServersListType } from "cfx/common/services/servers/lists/types";
import { IServersService } from "cfx/common/services/servers/servers.service";
import { IServersConnectService } from "cfx/common/services/servers/serversConnect.service";
import { IServersStorageService } from "cfx/common/services/servers/serversStorage.service";
import { registerSettingsService } from "cfx/common/services/settings/settings.common";
import {
  ISettingsService,
  ISettingsUIService,
} from "cfx/common/services/settings/settings.service";
import { IUiService } from "cfx/common/services/ui/ui.service";
import { timeout } from "cfx/utils/async";

import { MpMenuApp } from "./components/MpMenuApp/MpMenuApp";
import { mpMenu } from "./mpMenu";
import { Handle404 } from "./pages/404";
import Register from "./pages/Register/Register";
import {
  IAuthService,
  registerAuthService,
} from "./services/auth/auth.service";
import { registerConvarService } from "./services/convars/convars.service";
import { registerDiscourseService } from "./services/discourse/discourse.service";
import { registerMpMenuIntlService } from "./services/intl/intl.mpMenu";
import { registerLinkedIdentitiesService } from "./services/linkedIdentities/linkedIdentities.service";
import {
  IPlatformStatusService,
  registerPlatformStatusService,
} from "./services/platformStatus/platformStatus.service";
import { registerHomeScreenServerList } from "./services/servers/list/HomeScreenServerList.service";
import {
  MpMenuLocalhostServerService,
  registerMpMenuLocalhostServerService,
} from "./services/servers/localhostServer.mpMenu";
import { registerMpMenuServersConnectService } from "./services/servers/serversConnect.mpMenu";
import { registerMpMenuServersReviewsService } from "./services/servers/serversReviews.mpMenu";
import { registerMpMenuServersStorageService } from "./services/servers/serversStorage.mpMenu";
import { registerMpMenuUiService } from "./services/ui/ui.mpMenu";
import {
  IUiMessageService,
  registerUiMessageService,
} from "./services/uiMessage/uiMessage.service";
import { DEFAULT_GAME_SETTINGS_CATEGORY_ID, GAME_SETTINGS } from "./settings";
import { shutdownLoadingSplash } from "./utils/loadingSplash";
import {
  ILauncherService,
  registerLauncherService,
} from "./services/launcher/launcher.service";
import Login from "./pages/Login/Login";
import ChangePassword from "./pages/ChangePassword/ChangePassword";
import ForgotPassword from "./pages/ForgotPassword/ForgotPassword";

startBrowserApp({
  defineServices(container) {
    registerLogService(container, [ConsoleLogProvider]);

    registerSettingsService(container, {
      settings: GAME_SETTINGS,
      defaultSettingsCategoryId: DEFAULT_GAME_SETTINGS_CATEGORY_ID,
    });

    registerAuthService(container);
    registerConvarService(container);
    registerDiscourseService(container);
    registerUiMessageService(container);
    registerPlatformStatusService(container);
    registerLinkedIdentitiesService(container);

    registerHomeScreenServerList(container);

    registerMpMenuUiService(container);
    registerMpMenuIntlService(container);

    registerMpMenuServersService(container, {
      listTypes: [
        ServersListType.All,
        ServersListType.Supporters,
        ServersListType.History,
        ServersListType.Favorites,
      ],
    });
    registerMpMenuServersStorageService(container);
    registerMpMenuServersReviewsService(container);
    registerMpMenuServersConnectService(container);
    registerMpMenuLocalhostServerService(container);
    registerLauncherService(container);
  },

  beforeRender(container) {
    // Pre-resolve critical services
    [
      IUiService,
      IAuthService,
      IIntlService,
      IServersService,
      ISettingsService,
      IUiMessageService,
      ISettingsUIService,
      IPlatformStatusService,
      IServersStorageService,
      IServersConnectService,
      ILauncherService,

      MpMenuLocalhostServerService as any,
    ].forEach((serviceId) => container.get(serviceId));
  },

  render: () => (
    <HashRouter>
      <Routes>
        <Route path="" element={<MpMenuApp />}>
          {/* <Route index element={<HomePage />} /> */}
          <Route path="register" element={<Register />} />
          <Route path="login" element={<Login />} />
          <Route path="change-password" element={<ChangePassword />} />
          <Route path="forgot-password" element={<ForgotPassword />} />
          <Route path="*" element={<Handle404 />} />
        </Route>
      </Routes>
    </HashRouter>
  ),

  afterRender() {
    mpMenu.invokeNative("backfillEnable");
    mpMenu.invokeNative("loadWarning");

    if (__CFXUI_DEV__) {
      mpMenu.invokeNative("executeCommand", "nui_devtools mpMenu");
    }

    mpMenu.showGameWindow();

    // Not using await here so app won't wait for this to end
    timeout(1000).then(shutdownLoadingSplash);
  },
});
