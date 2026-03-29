/* eslint-disable @stylistic/operator-linebreak */
import { injectable, interfaces, multiInject, optional } from "inversify";
import React from "react";

import { AnalyticsProvider } from "./analytics.extensions";
import { IAnalyticsEvent, TrackEventParams } from "./types";
import {
  defineService,
  ServicesContainer,
  useService,
} from "../../../base/servicesContainer";

export const IAnalyticsService =
  defineService<IAnalyticsService>("AnalyticsService");
export interface IAnalyticsService {
  trackEvent(event: IAnalyticsEvent): void;
}

export function registerAnalyticsService(
  container: ServicesContainer,
  providers: interfaces.Newable<AnalyticsProvider>[] = []
) {
  container.registerImpl(IAnalyticsService, AnalyticsService);

  providers.forEach((provider) => {
    container.registerImpl(AnalyticsProvider, provider);
  });
}

@injectable()
class AnalyticsService implements IAnalyticsService {
  @multiInject(AnalyticsProvider)
  @optional()
  protected readonly providers: AnalyticsProvider[];

  trackEvent(event: IAnalyticsEvent) {
    this.providers.forEach((provider) => provider.trackEvent(event));
  }
}

export function useAnalyticsService(): IAnalyticsService {
  return useService(IAnalyticsService);
}

export function useEventHandler() {
  const eventHandler = React.useCallback((_params: TrackEventParams) => {}, []);

  return eventHandler;
}
