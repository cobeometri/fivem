import { Flex, FlexRestricter, Page } from "@cfx-dev/ui-components";
import { observer } from "mobx-react-lite";

import { InsideNavBar } from "cfx/apps/mpMenu/parts/NavBar/InsideNavBar";

import { Continuity } from "./Continuity/Continuity";
import { Footer } from "./Footer/Footer";
import { HomePageNavBarLinks } from "./HomePage.links";
import { PlatformStats } from "./PlatformStats/PlatformStats";
import { PlatformStatus } from "./PlatformStatus/PlatformStatus";
import { TopServersBlock } from "./TopServers/TopServers";

export const HomePage = observer(function HomePage() {
  return (
    <Page>
      <InsideNavBar>
        <Flex fullWidth spaceBetween gap="large">
          <HomePageNavBarLinks />

          <PlatformStatus />

          <PlatformStats />
        </Flex>
      </InsideNavBar>

      <Flex fullHeight gap="xlarge">
        <FlexRestricter>
          <Flex fullWidth fullHeight vertical repell gap="xlarge">
            <Continuity />

            <TopServersBlock />

            <Footer />
          </Flex>
        </FlexRestricter>
      </Flex>
    </Page>
  );
});
