exception UnexpectedResponse(int);

let handleApiError =
  [@bs.open]
  (
    fun
    | UnexpectedResponse(code) => code
  );

[%bs.raw {|require("./TimelineEventsPanel.scss")|}];

let str = ReasonReact.string;

type selectedTab =
  | PendingTab
  | CompletedTab;

type state = {selectedTab};

type action =
  | SwitchTab(selectedTab);

let component = ReasonReact.reducerComponent("TimelineEventsPanel");

let founderFilter = (founderId, tes) =>
  switch (founderId) {
  | None => tes
  | Some(id) => tes |> TimelineEvent.forFounderId(id)
  };

let loadMoreVisible = (selectedFounderId, selectedTab, hasMorePendingTEs, hasMoreCompletedTEs) =>
  selectedFounderId == None && (selectedTab == PendingTab ? hasMorePendingTEs : hasMoreCompletedTEs);

let handleResponseJSON = (state, hasMorePendingTEs, hasMoreCompletedTEs, appendTEsCB, json) =>
  switch (json |> Json.Decode.(field("error", nullable(string))) |> Js.Null.toOption) {
  | Some(error) => Notification.error("Something went wrong!", error)
  | None =>
    let newTEs = json |> Json.Decode.(field("timelineEvents", list(TimelineEvent.decode)));
    let moreToLoad = json |> Json.Decode.(field("moreToLoad", bool));
    let hasMorePendingTEs = state.selectedTab == PendingTab ? moreToLoad : hasMorePendingTEs;
    let hasMoreCompletedTEs = state.selectedTab == CompletedTab ? moreToLoad : hasMoreCompletedTEs;
    appendTEsCB(newTEs, hasMorePendingTEs, hasMoreCompletedTEs);
  };

let fetchEvents = (state, tes, hasMorePendingTEs, hasMoreCompletedTEs, appendTEsCB, courseId) => {
  let excludedIds =
    tes |> List.map(te => te |> TimelineEvent.id |> string_of_int) |> String.concat("&excludedIds[]=");
  let reviewStatus = state.selectedTab == PendingTab ? "pending" : "complete";
  let params = "limit=20&reviewStatus=" ++ reviewStatus ++ "&excludedIds[]=" ++ excludedIds;
  Js.Promise.(
    Fetch.fetch("/courses/" ++ (courseId |> string_of_int) ++ "/coach_dashboard/timeline_events?" ++ params)
    |> then_(response =>
         if (Fetch.Response.ok(response) || Fetch.Response.status(response) == 422) {
           response |> Fetch.Response.json;
         } else {
           Js.Promise.reject(UnexpectedResponse(response |> Fetch.Response.status));
         }
       )
    |> then_(json =>
         json |> handleResponseJSON(state, hasMorePendingTEs, hasMoreCompletedTEs, appendTEsCB) |> resolve
       )
    |> catch(error =>
         (
           switch (error |> handleApiError) {
           | Some(code) => Notification.error(code |> string_of_int, "Please try again")
           | None => Notification.error("Something went wrong", "Please try again")
           }
         )
         |> resolve
       )
    |> ignore
  );
};

let make =
    (
      ~timelineEvents,
      ~hasMorePendingTEs,
      ~hasMoreCompletedTEs,
      ~appendTEsCB,
      ~founders,
      ~selectedFounderId,
      ~replaceTimelineEvent,
      ~authenticityToken,
      ~emptyIconUrl,
      ~notAcceptedIconUrl,
      ~verifiedIconUrl,
      ~gradeLabels,
      ~passGrade,
      ~courseId,
      _children,
    ) => {
  ...component,
  initialState: () => {selectedTab: PendingTab},
  reducer: (action, _state) =>
    switch (action) {
    | SwitchTab(selectedTab) => ReasonReact.Update({selectedTab: selectedTab})
    },
  render: ({state, send}) => {
    let statusFilter =
      switch (state.selectedTab) {
      | PendingTab => TimelineEvent.reviewPending
      | CompletedTab => TimelineEvent.reviewComplete
      };
    let pendingCount =
      timelineEvents |> founderFilter(selectedFounderId) |> TimelineEvent.reviewPending |> List.length;
    let timelineEvents = timelineEvents |> founderFilter(selectedFounderId) |> statusFilter;
    <div className="timeline-events-panel__container pt-4">
      <div className="d-flex mb-3 timeline-events-panel__status-tab-container">
        <div
          className=(
            "timeline-events-panel__status-tab"
            ++ (state.selectedTab == PendingTab ? " timeline-events-panel__status-tab--active" : "")
          )
          onClick=(_e => send(SwitchTab(PendingTab)))>
          ("Pending" |> str)
          (
            if (pendingCount > 0) {
              <span className="badge"> (pendingCount |> string_of_int |> str) </span>;
            } else {
              ReasonReact.null;
            }
          )
        </div>
        <div
          className=(
            "timeline-events-panel__status-tab"
            ++ (state.selectedTab == CompletedTab ? " timeline-events-panel__status-tab--active" : "")
          )
          onClick=(_e => send(SwitchTab(CompletedTab)))>
          ("Reviewed" |> str)
        </div>
      </div>
      <TimelineEventsList
        timelineEvents
        founders
        replaceTimelineEvent
        authenticityToken
        notAcceptedIconUrl
        verifiedIconUrl
        gradeLabels
        passGrade
        emptyIconUrl
      />
      (
        if (loadMoreVisible(selectedFounderId, state.selectedTab, hasMorePendingTEs, hasMoreCompletedTEs)) {
          let buttonText = state.selectedTab == PendingTab ? "Load more" : "Load earlier reviews";
          <div
            className="btn btn-primary mb-3"
            onClick=(
              _e => fetchEvents(state, timelineEvents, hasMorePendingTEs, hasMoreCompletedTEs, appendTEsCB, courseId)
            )>
            <i className="fa fa-cloud-download mr-1" />
            (buttonText |> str)
          </div>;
        } else {
          ReasonReact.null;
        }
      )
    </div>;
  },
};
