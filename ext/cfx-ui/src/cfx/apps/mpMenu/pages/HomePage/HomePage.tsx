import { Flex, FlexRestricter, Page, Text, Title, Box } from '@cfx-dev/ui-components';
import { observer } from 'mobx-react-lite';

import { InsideNavBar } from 'cfx/apps/mpMenu/parts/NavBar/InsideNavBar';

import { Continuity } from './Continuity/Continuity';
import { Footer } from './Footer/Footer';
import { HomePageNavBarLinks } from './HomePage.links';
import { TopServersBlock } from './TopServers/TopServers';
import { PlatformStatus } from '../../parts/PlatformStatus/PlatformStatus';

export const HomePage = observer(function HomePage() {
  return (
    <Page>
      <InsideNavBar>
        <Flex fullWidth spaceBetween gap="large">
          <HomePageNavBarLinks />

          <PlatformStatus />

          <Flex centered="axis" gap="small">
            <Text size="small" opacity="50">Grand Roleplay</Text>
          </Flex>
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
