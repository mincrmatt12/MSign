import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import InputGroup from 'react-bootstrap/InputGroup'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Container from 'react-bootstrap/Container'
import Button from 'react-bootstrap/Button'

class ScInnerPane extends React.Component {
	constructor(props) {
		super(props);

		this.state = {
			start: this.displayTime(props.configState.startTime),
			end:   this.displayTime(props.configState.endTime),
			duration: (props.configState.duration / 1000).toString()
		}
	}

	parseTime(tm) {
		if (tm === "") return Number.NaN;

		let text = tm.trim();
		if (text.includes(":") && text.length == 5) {
			var hour, minute;
			console.log(text.split(":"));
			[hour, minute] = text.split(":").map((x) => Number.parseInt(x));
			console.log(hour, minute);

			if (Number.isNaN(hour) || Number.isNaN(minute)) return Number.NaN;

			return hour * 3600000 + minute * 60000;
		}
		else if (text.endsWith("h") || text.endsWith("H") && text.length == 3) {
			return Number.parseInt(text.substr(0, text.length - 1)) * 3600000
		}
		else {
			return Number.NaN;
		}
	}

	displayTime(tm) {
		if (Number.isNaN(tm)) return "";
		let hour = Math.floor(tm / 3600000), minute = Math.floor((tm % 60000) / 1000);

		return hour.toString().padStart(2, "0") + ":" + minute.toString().padStart(2, "0");
	}

	onChangeTime(x, e) {
		if (e == "startTime") this.setState({start: x});
		if (e == "endTime") this.setState({end: x});
		
		let v = this.parseTime(x);
		if (Number.isNaN(v)) return;
		let new_c = this.props.configState;

		new_c[e] = v;
		this.props.uploadState(new_c);
	}

	setDuration(x) {
		this.setState({duration: x});
		x = Number.parseFloat(x);
		if (Number.isNaN(x)) return;
		let new_c = this.props.configState;
		new_c.duration = Math.floor(x * 1000);
		this.props.uploadState(new_c);
	}

	setMode(x) {
		let new_c = this.props.configState;
		new_c.mode = x;
		this.props.uploadState(new_c);
	}

	render() {
		return <Card className="mb-3 p-1">
			<Card.Body>
				<Card.Title>{this.props.title}</Card.Title>
				<hr className="hr-darkgray" />
				<Form.Check type="radio">
					<Form.Check.Input type="radio" checked={this.props.configState.mode == 0} onChange={(e) => {if (e.target.checked) this.setMode(0);}}/>
					<Form.Check.Label><p> enabled </p></Form.Check.Label>
				</Form.Check>
				<Form.Check type="radio">
					<Form.Check.Input type="radio" checked={this.props.configState.mode == 1} onChange={(e) => {if (e.target.checked) this.setMode(1);}}/>
					<Form.Check.Label><p> disabled </p></Form.Check.Label>
				</Form.Check>
				<Form inline>
					<Form.Check type="radio">
						<Form.Check.Input type="radio" checked={this.props.configState.mode == 2} onChange={(e) => {if (e.target.checked) this.setMode(2);}}/>
						<Form.Check.Label>enabled from <Form.Control isInvalid={Number.isNaN(this.parseTime(this.state.start)) && this.props.configState.mode == 2} onChange={(e) => this.onChangeTime(e.target.value, "startTime")} 
								disabled={this.props.configState.mode != 2} className="mx-2" type="text" value={this.state.start} placeholder="HH:MM" /> to 
						<Form.Control isInvalid={Number.isNaN(this.parseTime(this.state.end)) && this.props.configState.mode == 2}onChange={(e) => this.onChangeTime(e.target.value, "endTime")}
								disabled={this.props.configState.mode != 2} className="ml-2" type="text" value={this.state.end} placeholder="HH:MM" />
						</Form.Check.Label>
					</Form.Check>
				</Form>

				<hr className="hr-darkgray" />

				<Form inline className="pb-0 mb-0">
					lasts for <Form.Control className="mx-2" value={this.state.duration} type="text" onChange={(e) => this.setDuration(e.target.value)} /> seconds
				</Form>
			</Card.Body>
		</Card>
	}
}

class ScCfgPane extends React.Component {
	constructor(props) {
		super(props)
	}

	upload(p, val) {
		let v_s = this.props['configState'];
		v_s[p] = val;
		this.props['updateState'](v_s);
	}

	render() {
		return <div>
			<hr className="hr-gray" />
			<ScInnerPane title="ttc" configState={this.props.configState.ttc} uploadState={(x) => this.upload("ttc", x)} />
			<ScInnerPane title="weather" configState={this.props.configState.weather} uploadState={(x) => this.upload("weather", x)} />
			<ScInnerPane title="3d model" configState={this.props.configState.model} uploadState={(x) => this.upload("model", x)} />
			<ScInnerPane title="school calendar" configState={this.props.configState.calfix} uploadState={(x) => this.upload("calfix", x)} />
			<hr className="hr-gray" />
		</div>
	}
}

export default ScCfgPane;

